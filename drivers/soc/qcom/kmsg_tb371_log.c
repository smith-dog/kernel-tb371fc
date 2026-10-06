// SPDX-License-Identifier: GPL-2.0
/*
 * kmsg_tb371_log - printk flight recorder for TB371FC, written straight to the
 * raw UFS 'logdump' area (no fs, no /data, no userspace, no SELinux).
 *
 * Why raw flash and not RAM (all measured on this board, TASK-039/TASK-058):
 *  - DDR is reinitialized on every reset (XBL refills its 0xAF zone), so no
 *    log_buf, ramoops or IMEM breadcrumb survives a reboot.
 *  - The EDL firehose programmer here implements 15 functions and neither
 *    peek nor poke is among them, so RAM cannot be read out over EDL either.
 *  - firehose <read> does work, so anything landed on storage is retrievable
 *    from a hung/bricked device through QFIL.
 *
 * Landing zone (LBAs read off the device GPT, then cross-checked by writing a
 * pattern via QFIL and reading it back through /dev/block/by-name/logdump):
 *  UFS LU4 == /dev/sde; 'logdump' = LBA 344177..360560 (64 MiB), unused.
 * This recorder owns LBA 344177..345200 (4 MiB): sector 0 = index, sectors
 * 1..1023 = one record per sector. Nothing outside that window is touched.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kmsg_dump.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/utsname.h>

#define KLOG_DEV		"/dev/sde"
#define KLOG_BASE_LBA		344177ULL
#define KLOG_SECT		4096U
#define KLOG_RING		1024U	/* sectors incl. index sector */
#define KLOG_MAGIC		0x31474F4C	/* "LOG1" */
#define KLOG_TEXT_MAX		(KLOG_SECT - 48)
#define KLOG_TAIL_MAX		3900U	/* kmsg bytes per record */
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
static struct file *devfile;
static struct task_struct *worker;
static struct kmsg_dumper dumper;
static unsigned long pagebuf;		/* one aligned sector buffer */
static char *tailbuf;
static u64 boot_count, seq;
static bool armed;

static int klog_write(u64 lba, const void *buf)
{
	loff_t pos = (loff_t)lba * KLOG_SECT;
	ssize_t w = kernel_write(devfile, buf, KLOG_SECT, &pos);

	return w == (ssize_t)KLOG_SECT ? 0 : (w < 0 ? (int)w : -EIO);
}

static int klog_read(u64 lba, void *buf)
{
	loff_t pos = (loff_t)lba * KLOG_SECT;
	ssize_t r = kernel_read(devfile, buf, KLOG_SECT, &pos);

	return r == (ssize_t)KLOG_SECT ? 0 : (r < 0 ? (int)r : -EIO);
}

/*
 * Land one record, then refresh the index sector, then flush. 'block' is false
 * on the panic path: a locked mutex there means another CPU is mid-write and
 * spinning on it would only lose the reboot.
 */
static void klog_commit(const char *text, size_t len, u32 reason, bool block)
{
	struct klog_index *ix = (void *)pagebuf;
	struct klog_record *rec = (void *)pagebuf;
	u64 slot;
	int rc = 0;

	if (!devfile)
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
	rc = klog_write(slot, rec);
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
	rc |= klog_write(KLOG_BASE_LBA, ix);

	vfs_fsync(devfile, 0);
	mutex_unlock(&klog_lock);

	if (rc)
		pr_emerg("kmsg_tb371_log: flash write failed rc=%d\n", rc);
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

static int klog_open(void)
{
	struct klog_index *ix = (void *)pagebuf;
	struct file *f;
	char banner[320];
	int n;

	f = filp_open(KLOG_DEV, O_RDWR | O_DIRECT, 0);
	if (IS_ERR(f))
		return PTR_ERR(f);
	devfile = f;

	boot_count = 0;
	if (klog_read(KLOG_BASE_LBA, ix) == 0 && ix->magic == KLOG_MAGIC)
		boot_count = ix->boot_count + 1;
	seq = 0;

	dumper.dump = klog_panic_dump;
	dumper.max_reason = KMSG_DUMP_PANIC;
	kmsg_dump_register(&dumper);
	armed = true;

	pr_info("kmsg_tb371_log: armed on " KLOG_DEV " boot #%llu\n", boot_count);
	n = scnprintf(banner, sizeof(banner),
		      "=== kmsg_tb371_log boot #%llu uts %s ===\n%s\n",
		      boot_count, init_utsname()->release, saved_command_line);
	klog_commit(banner, n, KLOG_R_PERIODIC, true);
	return 0;
}

static int klog_fn(void *arg)
{
	size_t n;

	while (!kthread_should_stop()) {
		if (!devfile) {
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
	if (devfile)
		filp_close(devfile, NULL);
	free_page(pagebuf);
	kfree(tailbuf);
}

module_init(kmsg_tb371_log_init);
module_exit(kmsg_tb371_log_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB371FC printk flight recorder on the UFS logdump area");
