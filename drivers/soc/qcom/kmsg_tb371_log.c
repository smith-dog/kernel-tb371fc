// SPDX-License-Identifier: GPL-2.0
/*
 * kmsg_tb371_log - printk flight recorder for TB371FC.
 *
 * Lands the tail of log_buf on raw UFS storage so a hung or bricked boot can
 * still be read out. Why this is the only channel on this board (all measured,
 * TASK-039 / TASK-058):
 *  - DDR is reinitialized on every reset (XBL refills its 0xAF zone), so no
 *    log_buf, ramoops or IMEM breadcrumb survives a reboot.
 *  - The EDL firehose programmer implements 15 functions and peek/poke are not
 *    among them (verified live), so RAM cannot be read over EDL either.
 *  - firehose <read> does work, so anything landed on storage is retrievable
 *    from a dead device through QFIL.
 *
 * I/O design notes (two bugs this form specifically avoids):
 *  - Addressed by dev_t + bio, not by a /dev path: Android keeps nodes under
 *    /dev/block/, and a built-in writer must not depend on init mounting /dev.
 *  - Plain buffered-O_DIRECT kernel_write is impossible here: 4.19
 *    kernel_write() pins user pages, so a kernel buffer would fault. bio with
 *    REQ_SYNC|REQ_FUA writes the sector straight to media instead.
 *  - The target LU is *found*, not assumed: it must be exactly 2 GiB (LUN4 =
 *    524288 x 4096) and the landing sector must still hold what we left there
 *    (all-zero, our LOG1 ring, or the p367 TASK test pattern). No match, no
 *    write - this driver must never scribble on an unknown disk.
 *
 * Ring layout at LUN4 LBA 344177 ('logdump', LBA 344177..360560, verified empty
 * and unused), 1024 sectors = 4 MiB: sector 0 = index, sectors 1..1023 = one
 * record per sector.
 */

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kmsg_dump.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/utsname.h>

#define KLOG_BASE_LBA		344177ULL	/* 'logdump' first_lba, read off GPT */
#define KLOG_LU_BYTES		(2ULL << 30)	/* LUN4 capacity guard */
#define KLOG_SECT		4096U
#define KLOG_SECTORS_512	(KLOG_SECT >> 9)
#define KLOG_RING		1024U		/* sectors incl. index */
#define KLOG_MAGIC		0x31474F4C	/* "LOG1" */
#define KLOG_TEXT_MAX		(KLOG_SECT - 48)
#define KLOG_TAIL_MAX		3900U
#define KLOG_INTERVAL_MS	1000

#define KLOG_R_PERIODIC		0
#define KLOG_R_PANIC		1

struct klog_index {
	u32 magic;
	u32 version;
	u64 boot_count;
	u64 epoch_real;
	u64 seq;
	u64 last_uptime_ms;
	u64 base_lba;
	u32 ring_sectors;
	u32 sector_size;
	u32 text_max;
	char uts[64];
} __packed;

struct klog_record {
	u32 magic;
	u32 boot_count_lo;
	u64 seq;
	u64 uptime_ms;
	u64 real_sec;
	u32 len;
	u32 reason;
	char text[KLOG_TEXT_MAX];
} __packed;

static DEFINE_MUTEX(klog_lock);
static void *klog_holder = &klog_holder;
static struct block_device *bdev;
static struct task_struct *worker;
static struct kmsg_dumper dumper;
static unsigned long pagebuf;
static char *tailbuf;
static u64 boot_count, seq;
static bool armed;

static int klog_bio(struct block_device *to, int op, u64 lba)
{
	struct bio *bio = bio_alloc(GFP_KERNEL, 1);
	int ret;

	bio_set_op_attrs(bio, op, REQ_SYNC | REQ_FUA);
	bio_set_dev(bio, to);
	bio->bi_iter.bi_sector = lba * KLOG_SECTORS_512;
	if (!bio_add_page(bio, virt_to_page(pagebuf), KLOG_SECT, 0))
		pr_warn_once("kmsg_tb371_log: bio_add_page truncation\n");
	ret = submit_bio_wait(bio);
	bio_put(bio);
	return ret;
}

static int klog_write(u64 lba)
{
	return klog_bio(bdev, REQ_OP_WRITE, lba);
}

static int klog_read_on(struct block_device *from, u64 lba)
{
	return klog_bio(from, REQ_OP_READ, lba);
}

static int klog_read(u64 lba)
{
	return klog_read_on(bdev, lba);
}

/*
 * Land one record, then refresh the index. 'block' is false on the panic path:
 * a held mutex there means another CPU is mid-write, and spinning on it would
 * only cost us the reboot.
 */
static void klog_commit(const char *text, size_t len, u32 reason, bool block)
{
	struct klog_index *ix = (void *)pagebuf;
	struct klog_record *rec = (void *)pagebuf;
	u64 slot;
	int rc;

	if (!bdev)
		return;
	if (len > KLOG_TEXT_MAX)
		len = KLOG_TEXT_MAX;

	if (block)
		mutex_lock(&klog_lock);
	else if (!mutex_trylock(&klog_lock))
		return;

	memset(rec, 0, KLOG_SECT);
	rec->magic = KLOG_MAGIC;
	rec->boot_count_lo = (u32)boot_count;
	rec->seq = seq;
	rec->uptime_ms = jiffies_to_msecs(jiffies);
	rec->real_sec = (u64)ktime_get_real_seconds();
	rec->len = len;
	rec->reason = reason;
	memcpy(rec->text, text, len);
	slot = KLOG_BASE_LBA + 1 + (seq % (KLOG_RING - 1));
	rc = klog_write(slot);
	seq++;

	memset(ix, 0, KLOG_SECT);
	ix->magic = KLOG_MAGIC;
	ix->version = 1;
	ix->boot_count = boot_count;
	ix->epoch_real = (u64)ktime_get_real_seconds();
	ix->seq = seq;
	ix->last_uptime_ms = jiffies_to_msecs(jiffies);
	ix->base_lba = KLOG_BASE_LBA;
	ix->ring_sectors = KLOG_RING;
	ix->sector_size = KLOG_SECT;
	ix->text_max = KLOG_TEXT_MAX;
	strscpy(ix->uts, init_utsname()->release, sizeof(ix->uts));
	rc |= klog_write(KLOG_BASE_LBA);
	mutex_unlock(&klog_lock);

	if (rc)
		pr_emerg("kmsg_tb371_log: bio write failed rc=%d\n", rc);
}

/* Newest KLOG_TAIL_MAX bytes of log_buf, first (partial) line dropped. */
static size_t klog_kmsg(char *out, size_t cap)
{
	struct kmsg_dumper it;
	size_t len = 0, off = 0;

	memset(&it, 0, sizeof(it));
	it.active = true;
	kmsg_dump_rewind(&it);
	if (!kmsg_dump_get_buffer(&it, false, tailbuf, KLOG_TAIL_MAX, &len))
		return 0;
	while (off < len && tailbuf[off] != '\n')
		off++;
	if (off >= len)
		return 0;
	len -= off;
	if (len > cap)
		len = cap;
	memcpy(out, tailbuf + off, len);
	return len;
}

/*
 * The tail snapshot alone (3.9 KB) scrolls the panic trigger line out of the
 * window when the panic output is long (multi-CPU stopping dumps). Grab up to
 * 16 KB at panic time and commit it as consecutive records so the trigger
 * always lands in the ring.
 */
static char panicbuf[16384] __aligned(4);

static void klog_panic_dump(struct kmsg_dumper *d, enum kmsg_dump_reason reason)
{
	size_t len = 0, off = 0;
	struct kmsg_dumper it;

	if (reason != KMSG_DUMP_PANIC || !armed)
		return;

	memset(&it, 0, sizeof(it));
	it.active = true;
	kmsg_dump_rewind(&it);
	if (!kmsg_dump_get_buffer(&it, false, panicbuf, sizeof(panicbuf), &len))
		return;
	while (off < len && panicbuf[off] != '
')
		off++;
	if (off >= len)
		return;
	while (off < len) {
		size_t c = min_t(size_t, KLOG_TEXT_MAX, len - off);

		klog_commit(panicbuf + off, c, KLOG_R_PANIC, false);
		off += c;
	}
}

/* Is this sector one that we are allowed to own? */
static bool klog_sector_ours(struct block_device *from)
{
	u32 *w = (void *)pagebuf;
	int i, all_zero = 1;

	if (klog_read_on(from, KLOG_BASE_LBA)) {
		pr_info("kmsg_tb371_log: landing sector %llu read failed\n",
			KLOG_BASE_LBA);
		return false;
	}
	if (w[0] == KLOG_MAGIC)
		return true;			/* our own ring */
	if (!memcmp((void *)pagebuf, "TASK", 4))
		return true;			/* p367 round-trip test pattern */
	for (i = 0; i < KLOG_SECT; i++)
		if (((char *)pagebuf)[i]) {
			all_zero = 0;
			break;
		}
	return all_zero;
}

/*
 * Locate the LU that owns our landing zone.
 *
 * Path-based resolution is impossible here: SELinux denies the kernel domain
 * blk_file read/write on /dev/block/sd*, and that check fires even for
 * lookup_bdev(), which does an inode_permission(MAY_READ|MAY_WRITE) (measured -
 * avc denied for every node, scontext=u:r:kernel:s0). Minor numbers cannot be
 * guessed either - sd minors come from an IDR, and /sys/block/sde/dev is 8:64,
 * not 8:32. So the built-in recorder walks the block class (which it may
 * reference directly; it is not exported to modules) and validates identity.
 *
 * A .ko build takes the dev_t from a module parameter instead - read it with
 * `cat /sys/block/<disk>/dev`. Both forms then pass the same gate: 2 GiB
 * capacity, 4096-byte logical blocks, and the landing sector still holding
 * what we know is there. No match, no write - this driver never scribbles on
 * an unverified disk.
 */
static bool klog_validate(struct block_device *b)
{
	u32 *w = (void *)pagebuf;
	bool cap_ok, sect_ok;

	cap_ok = get_capacity(b->bd_disk) == (sector_t)(KLOG_LU_BYTES >> 9);
	sect_ok = klog_sector_ours(b);
	if (!cap_ok || !sect_ok)
		pr_info("kmsg_tb371_log: gate %s%s cap=%llu want=%llu sect_first=%#x\n",
			cap_ok ? "" : "CAPACITY ",
			sect_ok ? "" : "SECTOR_CONTENT ",
			(u64)get_capacity(b->bd_disk),
			(u64)(KLOG_LU_BYTES >> 9), w[0]);
	/*
	 * Logical block size is deliberately not part of the gate: bios are
	 * addressed in 512-byte units regardless, and this tree leaves
	 * bd_block_size unset for these LUs (512 reported vs 4096 in sysfs).
	 */
	return cap_ok && sect_ok;
}

/* Claim one dev_t and put it through the identity gate. */
static bool klog_claim(dev_t dev, struct block_device **out)
{
	struct block_device *b = blkdev_get_by_dev(dev, FMODE_READ | FMODE_WRITE,
						   klog_holder);

	if (IS_ERR(b))
		return false;
	if (!klog_validate(b)) {
		blkdev_put(b, FMODE_READ | FMODE_WRITE);
		return false;
	}
	*out = b;
	return true;
}

#ifndef MODULE

static int klog_probe_dev(struct device *dev, void *data)
{
	struct block_device **hit = data;
	struct gendisk *disk;
	dev_t cand;

	/*
	 * block_class holds partitions too, whose parent is the disk device.
	 * dev_to_disk() is a container_of on part0.__dev and would decode a
	 * partition device into garbage, so skip anything parented inside the
	 * same class.
	 */
	if (dev->parent && dev->parent->class == &block_class)
		return 0;
	disk = dev_to_disk(dev);
	if (!disk)
		return 0;
	cand = MKDEV(disk->major, disk->first_minor);
	if (!klog_claim(cand, hit))
		return 0;
	pr_info("kmsg_tb371_log: LU %s (%d:%d) claimed by class walk\n",
		disk->disk_name, MAJOR(cand), MINOR(cand));
	return 1;
}

/*
 * Built-in form: try the dev_t the .ko positive control already proved on this
 * board first, so the treatment exercises exactly the code path the control
 * validated; the class walk stays as the fallback if sd enumerates differently.
 * Every candidate still passes klog_validate(), so a wrong dev_t can only be
 * rejected, never written to.
 */
static struct block_device *klog_find_lu(void)
{
	static const dev_t probes[] = { MKDEV(8, 64), MKDEV(8, 32), MKDEV(8, 40) };
	struct block_device *b = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(probes); i++)
		if (klog_claim(probes[i], &b)) {
			pr_info("kmsg_tb371_log: LU claimed by dev_t %d:%d\n",
				MAJOR(probes[i]), MINOR(probes[i]));
			return b;
		}
	/*
	 * block_class is registered by genhd_device_init() at subsys_initcall
	 * (level 1). Walking it earlier makes class_for_each_device() WARN out of
	 * its own bootstrap check, so only fall back once the class exists.
	 */
	if (block_class.p)
		class_for_each_device(&block_class, NULL, &b, klog_probe_dev);
	return b;
}

#else /* MODULE - positive-control build only */

static char *devnode = "8:64";
module_param(devnode, charp, 0444);
MODULE_PARM_DESC(devnode, "major:minor of the UFS LU holding the landing zone");

static struct block_device *klog_find_lu(void)
{
	struct block_device *b = NULL;
	unsigned int maj, min;

	if (sscanf(devnode, "%u:%u", &maj, &min) != 2)
		return NULL;
	if (!klog_claim(MKDEV(maj, min), &b)) {
		pr_err("kmsg_tb371_log: %u:%u is not the landing LU (or is busy)\n",
		       maj, min);
		return NULL;
	}
	pr_info("kmsg_tb371_log: LU claimed via devnode param %u:%u\n", maj, min);
	return b;
}
#endif

static int klog_open(void)
{
	struct klog_index *ix = (void *)pagebuf;
	char banner[256];
	int n;

	bdev = klog_find_lu();
	if (!bdev)
		return -ENODEV;

	boot_count = 0;
	seq = 0;
	if (!klog_read(KLOG_BASE_LBA) && ix->magic == KLOG_MAGIC) {
		boot_count = ix->boot_count + 1;
		/* Continue the cursor where the previous boot left off: a fresh boot
		 * must not erase the dead kernel's last records.
		 */
		seq = ix->seq;
	}

	dumper.dump = klog_panic_dump;
	dumper.max_reason = KMSG_DUMP_PANIC;
	kmsg_dump_register(&dumper);
	armed = true;

	n = scnprintf(banner, sizeof(banner),
		      "=== kmsg_tb371_log boot #%llu uts %s ===\n",
		      boot_count, init_utsname()->release);
	klog_commit(banner, n, KLOG_R_PERIODIC, true);
	pr_info("kmsg_tb371_log: armed, boot #%llu\n", boot_count);
	return 0;
}

static int klog_fn(void *arg)
{
	size_t n;

	while (!kthread_should_stop()) {
		if (!bdev) {
			klog_open();
		} else {
			n = klog_kmsg(tailbuf, KLOG_TEXT_MAX);
			if (n)
				klog_commit(tailbuf, n, KLOG_R_PERIODIC, true);
		}
		msleep_interruptible(KLOG_INTERVAL_MS);
	}
	return 0;
}

static int __init kmsg_tb371_log_init(void)
{
	pagebuf = get_zeroed_page(GFP_KERNEL);
	tailbuf = kmalloc(KLOG_TAIL_MAX, GFP_KERNEL);
	if (!pagebuf || !tailbuf) {
		free_page(pagebuf);
		kfree(tailbuf);
		return -ENOMEM;
	}
	worker = kthread_run(klog_fn, NULL, "kmsg_tb371_log");
	if (IS_ERR(worker)) {
		worker = NULL;
		free_page(pagebuf);
		kfree(tailbuf);
		pagebuf = 0;
		tailbuf = NULL;
		return PTR_ERR(worker);
	}
	pr_info("kmsg_tb371_log: recorder thread started\n");
	return 0;
}

static void __exit kmsg_tb371_log_exit(void)
{
	if (worker) {
		kthread_stop(worker);
		worker = NULL;
	}
	if (armed)
		kmsg_dump_unregister(&dumper);
	if (bdev) {
		blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
		bdev = NULL;
	}
	free_page(pagebuf);
	kfree(tailbuf);
}

#ifdef MODULE
module_init(kmsg_tb371_log_init);
module_exit(kmsg_tb371_log_exit);
#else
/* device_initcall (level 6) is the level proven on hardware by build #219
 * (claim at 1.441 s). early_initcall was tried in #220 and its worker thread was
 * simply gone after boot with the ring untouched, cause not pinned; see
 * state/STATE-058-*.md 2026-10-06 23:4x.
 */
device_initcall(kmsg_tb371_log_init);
#endif
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB371FC printk flight recorder on the UFS logdump area");
