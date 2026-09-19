#!/bin/bash
# v9c: correct API (MEMORY_DUMP_V2), rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p9c.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1
python3 - <<'PYEOF'
p = "/home/smith/kernel-van/kernel/printk/printk.c"
s = open(p, encoding="utf-8").read()
i = s.find("/* TB371FC_FORENSIC")
if i >= 0:
    s = s[:i]
    open(p, "w", encoding="utf-8").write(s)
    print("stripped")
else:
    print("no block")
PYEOF

cat >> kernel/printk/printk.c <<'EOFC'

/* TB371FC_FORENSIC: register kernel log buffer into QC minidump (V2 table) */
#ifdef CONFIG_QCOM_MEMORY_DUMP_V2
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
	rc = msm_dump_data_register(MSM_DUMP_TABLE_APPS, d);
	pr_info("TB371FC: KDMSG minidump register rc=%d len=%u\n", rc, log_buf_len);
	return rc;
}
late_initcall(tb371fc_kdmsg_dump_init);
#endif

/* TB371FC_FORENSIC: force panic at +60s so minidump/WD capture the state */
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
echo patched

./scripts/config -e QCOM_MEMORY_DUMP_V2 -e QCOM_MEMORY_DUMP -e QCOM_MINIDUMP \
  -e SOFTLOCKUP_DETECTOR -e BOOTPARAM_SOFTLOCKUP_PANIC -e DETECT_HUNG_TASK -e BOOTPARAM_HUNG_TASK_PANIC
./scripts/config --set-val PANIC_TIMEOUT 5
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "QCOM_MEMORY_DUMP|QCOM_MINIDUMP|SOFTLOCKUP_DETECTOR=|DETECT_HUNG_TASK=|PANIC_TIMEOUT" .config | head -8

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p9c-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p9c-make.log | head -6

if [ -f arch/arm64/boot/Image ] && [ arch/arm64/boot/Image -nt kernel/printk/printk.c ]; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK
else
  echo BUILD_FAILED_OR_STALE
fi
echo V9C_DONE
