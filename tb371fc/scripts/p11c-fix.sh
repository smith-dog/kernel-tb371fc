#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p11c.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
cp /home/smith/kernel-200/drivers/gpu/drm/drm_notifier_mi.c drivers/gpu/drm/drm_notifier_mi.c && echo notifier_c_ok
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p11c-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|No rule" $BASE/logs/p11c-make.log | head -8
if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v11
  echo BUILD_OK_WITH_FORENSICS
else
  echo BUILD_FAILED
fi
echo P11C_DONE
