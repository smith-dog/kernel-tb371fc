#!/bin/bash
# v9b: fix printk patch (correct API + balanced ifdefs), rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p9b.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1
# strip our broken appended block (from marker to EOF)
python3 - <<'PYEOF'
p = "/home/smith/kernel-van/kernel/printk/printk.c"
s = open(p, encoding="utf-8").read()
i = s.find("/* TB371FC_FORENSIC")
if i >= 0:
    s = s[:i]
    open(p, "w", encoding="utf-8").write(s)
    print("stripped old block")
else:
    print("no old block")
PYEOF

# append corrected block
cat >> kernel/printk/printk.c <<'EOFC'

/* TB371FC_FORENSIC: register kernel log buffer into QC minidump table */
#ifdef CONFIG_QCOM_MINIDUMP
#include <soc/qcom/memory_dump.h>
static int __init tb371fc_kdmsg_dump_init(void)
{
	struct msm_dump_data *d;
	int rc;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	strlcpy(d->name, "KDMSG", sizeof(d->name));
	d->addr = virt_to_phys(log_buf);
	d->len = log_buf_len;
	rc = msm_dump_data_register_minidump(MSM_DUMP_TABLE_APPS, d);
	pr_info("TB371FC: KDMSG minidump register rc=%d len=%u\n", rc, log_buf_len);
	return rc;
}
late_initcall(tb371fc_kdmsg_dump_init);
#endif

/* TB371FC_FORENSIC: force panic at +60s so WD/minidump capture the state */
static int tb371fc_forensic_thread(void *unused)
{
	ssleep(60);
	panic("TB371FC_FORENSIC_DUMP_TRIGGER");
	return 0;
}
static int __init tb371fc_forensic_init(void)
{
	pr_info("TB371FC forensic: arming 60s panic timer\n");
	kthread_run(tb371fc_forensic_thread, NULL, "tb371fc-forensic");
	return 0;
}
late_initcall(tb371fc_forensic_init);
EOFC
echo "patched"

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p9b-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p9b-make.log | head -6

if [ -f arch/arm64/boot/Image ] && [ arch/arm64/boot/Image -nt $OUT/Image-200 ]; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK
else
  echo BUILD_FAILED_OR_STALE
fi
echo V9B_DONE
