#!/bin/bash
# v9d (rewritten): verify printk patch state, config, build, repack
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p9d.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1

echo "=== printk.c forensic block state ==="
grep -n "TB371FC_FORENSIC\|msm_dump_data_register\|CONFIG_QCOM_MEMORY_DUMP_V2" kernel/printk/printk.c | head -8

echo "=== fix config (V2 only, no shadowing MEMORY_DUMP) ==="
./scripts/config -e QCOM_MEMORY_DUMP_V2 -e QCOM_MINIDUMP -d QCOM_MEMORY_DUMP \
  -e SOFTLOCKUP_DETECTOR -e BOOTPARAM_SOFTLOCKUP_PANIC -e DETECT_HUNG_TASK -e BOOTPARAM_HUNG_TASK_PANIC
./scripts/config --set-val PANIC_TIMEOUT 5
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "QCOM_MEMORY_DUMP|QCOM_MINIDUMP|SOFTLOCKUP_DETECTOR=|DETECT_HUNG_TASK=|PANIC_TIMEOUT" .config | head -8

echo "=== build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p9d-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p9d-make.log | head -6

if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK_WITH_FORENSICS
elif [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-v9
  echo BUILD_OK_NO_FORENSICS_MARKER
else
  echo BUILD_FAILED
fi
echo V9D_DONE
