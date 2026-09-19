#!/bin/bash
# v7e: copy LOS DRM core headers, rebuild vanilla + display stack
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7e.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
cp $K2/include/drm/drm_bridge.h $KV/include/drm/drm_bridge.h && echo bridge_ok
cp $K2/include/drm/drm_mipi_dsi.h $KV/include/drm/drm_mipi_dsi.h && echo mipi_ok
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7e-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|fatal error" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7e-make.log | head -8
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7E_DONE
