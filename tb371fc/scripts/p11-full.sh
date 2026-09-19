#!/bin/bash
# p11: full deterministic restore + rebuild of the TB371FC forensic kernel (v11)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p11-full.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

############ 0. toolchain ############
apt-get install -y llvm-17 clang-17 lld-17 device-tree-compiler cpio zstd \
  binutils-aarch64-linux-gnu libssl-dev libelf-dev bc python3 >> /dev/null 2>&1
ls /usr/lib/llvm-17/bin/clang >/dev/null 2>&1 && echo TOOLCHAIN_OK || echo TOOLCHAIN_MISSING
mkdir -p /usr/local/arm-as
ln -sf /usr/bin/aarch64-linux-gnu-as /usr/local/arm-as/as

############ 1. restore kernel-van (vanilla QC c25 4.19.157) ############
rm -rf /home/smith/kernel-van
mkdir -p /home/smith/kernel-van
cp -a $BASE/reference/msm-4.19-van/.git /home/smith/kernel-van/.git
cd /home/smith/kernel-van || exit 1
git config --global --add safe.directory /home/smith/kernel-van
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -1
head -5 Makefile | grep SUBLEVEL

############ 2. restore kernel-200 (LOS 20; display stack + headers source) ############
rm -rf /home/smith/kernel-200
mkdir -p /home/smith/kernel-200
cp -a $BASE/reference/k20/.git /home/smith/kernel-200/.git
cd /home/smith/kernel-200 || exit 1
git config --global --add safe.directory /home/smith/kernel-200
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -1

############ 3. kernel-van fixes + display port + forensic ############
cd /home/smith/kernel-van || exit 1

# 3a. trace include fixes at source level (clang-safe absolute paths)
sed -i 's|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH /home/smith/kernel-van/drivers/hid|' drivers/hid/hid-trace.h

# 3b. RTIC section-attribute removal (link relocation overflow fix)
sed -i 's/ __rticdata;/;/' security/selinux/hooks.c

# 3c. display stack port from LOS 20
rm -rf techpack/display
cp -a /home/smith/kernel-200/techpack/display techpack/display
rm -f techpack/display/built-in.a
find techpack/display -name ".*.cmd" -delete
# pll trace path + pll Makefile clean form (from proven fixed copy)
cp /home/smith/kernel-200/techpack/display/pll/Makefile techpack/display/pll/Makefile
sed -i 's|define TRACE_INCLUDE_PATH /home/smith/kernel-200/techpack/display/pll|define TRACE_INCLUDE_PATH /home/smith/kernel-van/techpack/display/pll|' techpack/display/pll/pll_trace.h
# MI/LOS headers + backlight impl
cp /home/smith/kernel-200/include/drm/drm_bridge.h include/drm/drm_bridge.h
cp /home/smith/kernel-200/include/drm/drm_mipi_dsi.h include/drm/drm_mipi_dsi.h
cp /home/smith/kernel-200/include/drm/drm_notifier_mi.h include/drm/drm_notifier_mi.h
cp /home/smith/kernel-200/include/linux/backlight.h include/linux/backlight.h
cp /home/smith/kernel-200/drivers/video/backlight/backlight.c drivers/video/backlight/backlight.c
# drm-y += notifier entry
grep -q "drm_notifier_mi" drivers/gpu/drm/Makefile || echo 'obj-y += drm_notifier_mi.o' >> drivers/gpu/drm/Makefile
# clk-debug CFLAGS (in case)
grep -q "CFLAGS_clk-debug" drivers/clk/qcom/Makefile 2>/dev/null || echo 'CFLAGS_clk-debug.o := -I$(src)' >> drivers/clk/qcom/Makefile

# 3d. printk forensic block (V2 entry API + 60s panic timer)
python3 - <<'PYEOF'
p = "/home/smith/kernel-van/kernel/printk/printk.c"
s = open(p, encoding="utf-8").read()
i = s.find("/* TB371FC_FORENSIC")
if i >= 0:
    s = s[:i]
    open(p, "w", encoding="utf-8").write(s)
block = """
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
	pr_info("TB371FC: KDMSG minidump register rc=%d addr=%px len=%u\\n",
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
	pr_info("TB371FC forensic: arming 60s panic timer\\n");
	kthread_run(tb371fc_forensic_thread, NULL, "tb371fc-forensic");
	return 0;
}
late_initcall(tb371fc_forensic_init);
"""
open(p, "w", encoding="utf-8").write(s + block)
print("printk forensic block applied")
PYEOF

############ 4. config ############
DEF=$(ls arch/arm64/configs/vendor/kona-perf_defconfig arch/arm64/configs/kona-perf_defconfig 2>/dev/null | head -1)
cp $DEF .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
./scripts/config -e PS5169 -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE \
  -e PID_NS -e IPC_NS -e USER_NS -e CGROUP_NS -e KPROBES \
  -e QCOM_MEMORY_DUMP_V2 -e QCOM_MINIDUMP -d QCOM_MEMORY_DUMP \
  -e SOFTLOCKUP_DETECTOR -e BOOTPARAM_SOFTLOCKUP_PANIC -e DETECT_HUNG_TASK -e BOOTPARAM_HUNG_TASK_PANIC
./scripts/config --set-val PANIC_TIMEOUT 5
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "^CONFIG_(PS5169|PSTORE=|PSTORE_RAM|PID_NS|IPC_NS|USER_NS|CGROUP_NS|KPROBES|QCOM_MEMORY_DUMP_V2|QCOM_MINIDUMP|SOFTLOCKUP_DETECTOR=|ARCH_QCOM)" .config | head -14

############ 5. build ############
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p11-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p11-make.log | head -6

if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v11
  echo BUILD_OK_WITH_FORENSICS
else
  echo BUILD_FAILED
fi

############ 6. images ############
# v11 main (ZUI ramdisk)
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v11 $OUT/boot-v11.img $OUT/dtbs/tail-new.bin
# v11 reader (mini ramdisk)
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v11 $OUT/boot-v11-reader.img $OUT/dtbs/tail-new.bin $OUT/mini-ramdisk3.img
# APatch with ramoops-reserved DTB (post-capture protected boot)
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/apatch-kernel.bin $OUT/boot-apatch-ramoops.img $OUT/dtbs/tail-new.bin
ls -la $OUT/ | grep -E "v11|apatch"
echo P11_FULL_DONE
