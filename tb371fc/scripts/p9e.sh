#!/bin/bash
# v9e: V2 entry-API KDMSG registration + panics, rebuild + repack
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p9e.log 2>&1
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

/* TB371FC_FORENSIC: register kernel log buffer into QC minidump V2 table */
#ifdef CONFIG_QCOM_MEMORY_DUMP_V2
#include <soc/qcom/memory_dump.h>
static struct msm_dump_data tb371fc_kdmsg_data;
static int __init tb371fc_kdmsg_dump_init(void)
{
	struct msm_dump_entry entry;
	int rc;

	memset(&tb371fc_kdmsg_data, 0, sizeof(tb371fc_kdmsg_data));
	strlcpy(tb371fc_kdmsg_data.name, "KDMSG", sizeof(tb371fc_kdmsg_data.name));
	tb371fc_kdmsg_data.addr = virt_to_phys(log_buf);
	tb371fc_kdmsg_data.len = log_buf_len;

	memset(&entry, 0, sizeof(entry));
	entry.id = MSM_DUMP_TABLE_APPS;
	strlcpy(entry.name, "KDMSG", sizeof(entry.name));
	entry.type = MSM_DUMP_DATA_MISC;
	entry.addr = virt_to_phys(log_buf);
	rc = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &entry);
	pr_info("TB371FC: KDMSG minidump register rc=%d addr=%px len=%u\n",
		rc, virt_to_phys(log_buf), log_buf_len);
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

./scripts/config -e QCOM_MEMORY_DUMP_V2 -e QCOM_MINIDUMP -d QCOM_MEMORY_DUMP \
  -e SOFTLOCKUP_DETECTOR -e BOOTPARAM_SOFTLOCKUP_PANIC -e DETECT_HUNG_TASK -e BOOTPARAM_HUNG_TASK_PANIC
./scripts/config --set-val PANIC_TIMEOUT 5
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "QCOM_MEMORY_DUMP|QCOM_MINIDUMP|SOFTLOCKUP_DETECTOR=|DETECT_HUNG_TASK=|PANIC_TIMEOUT" .config | head -8

echo "=== build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p9e-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p9e-make.log | head -6

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK
else
  echo BUILD_FAILED
fi

echo "=== repack boot-v9.img ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v9 $OUT/boot-v9.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v9.img
echo V9E_DONE
