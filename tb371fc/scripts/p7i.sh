#!/bin/bash
# v7i: add drm_notifier_mi.o to vanilla drm-y, rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7i.log 2>&1
echo START
KV=/home/smith/kernel-van
M=$KV/drivers/gpu/drm/Makefile
grep -q "drm_notifier_mi" $M || echo 'obj-y += drm_notifier_mi.o' >> $M
tail -2 $M
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7i-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7i-make.log | head -6
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7I_DONE
