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
static struct block_device *bdev;
static struct task_struct *worker;
static struct kmsg_dumper dumper;
static unsigned long pagebuf;
static char *tailbuf;
static u64 boot_count, seq;
static bool armed;

static int klog_bio(int op, u64 lba)
{
	struct bio *bio = bio_alloc(GFP_KERNEL, 1);
	int ret;

	bio_set_op_attrs(bio, op, REQ_SYNC | REQ_FUA);
	bio_set_dev(bio, bdev);
	bio->bi_iter.bi_sector = lba * KLOG_SECTORS_512;
	if (!bio_add_page(bio, virt_to_page(pagebuf), KLOG_SECT, 0))
		pr_warn_once("kmsg_tb371_log: bio_add_page truncation\n");
	ret = submit_bio_wait(bio);
	bio_put(bio);
	return ret;
}

static int klog_write(u64 lba)
{
	return klog_bio(REQ_OP_WRITE, lba);
}

static int klog_read(u64 lba)
{
	return klog_bio(REQ_OP_READ, lba);
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

static void klog_panic_dump(struct kmsg_dumper *d, enum kmsg_dump_reason reason)
{
	size_t n;

	if (reason != KMSG_DUMP_PANIC || !armed)
		return;
	n = klog_kmsg(tailbuf, KLOG_TEXT_MAX);
	klog_commit(tailbuf, n, KLOG_R_PANIC, false);
}

/* Is this sector one that we are allowed to own? */
static bool klog_sector_ours(void)
{
	u32 *w = (void *)pagebuf;
	int i, all_zero = 1;

	if (klog_read(KLOG_BASE_LBA))
		return false;
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
 * Find the UFS LU that owns our landing zone: exactly 2 GiB, and the landing
 * sector must be zero / ours / the known test pattern. Anything else is not
 * this device and we do not write to it.
 */
static struct block_device *klog_find_lu(void)
{
	struct block_device *b;
	int i;

	for (i = 0; i < 8; i++) {
		b = blkdev_get_by_dev(MKDEV(8, i * 8),
				      FMODE_READ | FMODE_WRITE, THIS_MODULE);
		if (IS_ERR(b))
			continue;
		if (i_size_read(b->bd_inode) != KLOG_LU_BYTES ||
		    bdev_logical_block_size(b) != KLOG_SECT || !klog_sector_ours()) {
			blkdev_put(b, FMODE_READ | FMODE_WRITE);
			continue;
		}
		pr_info("kmsg_tb371_log: LU found at %d:%d, LBA %llu reserved\n",
			MAJOR(b->bd_dev), MINOR(b->bd_dev), KLOG_BASE_LBA);
		return b;
	}
	return NULL;
}

static int klog_open(void)
{
	struct klog_index *ix = (void *)pagebuf;
	char banner[256];
	int n;

	bdev = klog_find_lu();
	if (!bdev)
		return -ENODEV;

	boot_count = 0;
	if (!klog_read(KLOG_BASE_LBA) && ix->magic == KLOG_MAGIC)
		boot_count = ix->boot_count + 1;
	seq = 0;

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
	if (bdev)
		blkdev_put(bdev, FMODE_READ | FMODE_WRITE);
	free_page(pagebuf);
	kfree(tailbuf);
}

module_init(kmsg_tb371_log_init);
module_exit(kmsg_tb371_log_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB371FC printk flight recorder on the UFS logdump area");
