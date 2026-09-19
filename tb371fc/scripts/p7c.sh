#!/bin/bash
# v7c: copy Xiaomi display core deps from 23.2 tree, rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7c.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
cp $K2/include/drm/drm_notifier_mi.h $KV/include/drm/drm_notifier_mi.h && echo h1_ok
cp $K2/include/linux/backlight.h $KV/include/linux/backlight.h && echo h2_ok
cp $K2/drivers/video/backlight/backlight.c $KV/drivers/video/backlight/backlight.c && echo c_ok
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7c-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|fatal error|undefined reference|Error [0-9]" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7c-make.log | head -10
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7C_DONE
