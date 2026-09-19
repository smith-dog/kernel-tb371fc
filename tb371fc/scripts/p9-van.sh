#!/bin/bash
# v9: vanilla 157 + display stack + QCOM_MINIDUMP(KDMSG) + lockup/hung panics + forensic timer
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p9-van.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1

echo "=== config: minidump + lockup/hung panics ==="
./scripts/config -e QCOM_MINIDUMP -e SOFTLOCKUP_DETECTOR -e BOOTPARAM_SOFTLOCKUP_PANIC \
  -e DETECT_HUNG_TASK -e BOOTPARAM_HUNG_TASK_PANIC
./scripts/config --set-val PANIC_TIMEOUT 5
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "QCOM_MINIDUMP|SOFTLOCKUP|HUNG_TASK|PANIC_TIMEOUT" .config | head -8

echo "=== patch printk.c: KDMSG minidump registration + 60s forensic panic ==="
if ! grep -q "TB371FC_FORENSIC" kernel/printk/printk.c; then
cat >> kernel/printk/printk.c <<'EOFC'

/* TB371FC_FORENSIC: register kernel log buffer into QC minidump table
 * so a stock kernel with QCOM_MINIDUMP can extract it after crash reboot. */
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
	rc = msm_dump_data_register(MSM_DUMP_TABLE_APPS, d);
	pr_info("TB371FC: KDMSG minidump register rc=%d addr=%px len=%u\n",
		rc, virt_to_phys(log_buf), log_buf_len);
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
#endif
EOFC
echo "printk patched"
else
echo "printk already patched"
fi

echo "=== build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p9-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p9-make.log | head -6

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK
else
  echo BUILD_FAILED
fi

echo "=== repack boot-v9.img ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v9 $OUT/boot-v9.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v9.img
echo V9_DONE
