#!/bin/bash
# v7b: apply display-port fixes WITHOUT re-copying the tree, then build
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7b.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KDIR=/home/smith/kernel-van
cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

echo "=== 1. copy 23.2 sde_drm.h (FOD macros) ==="
cp /home/smith/kernel-200/include/uapi/drm/sde_drm.h include/uapi/drm/sde_drm.h
grep -c "FOD_PRESSED_LAYER_ZORDER" include/uapi/drm/sde_drm.h

echo "=== 2. drop Xiaomi clone_cooling_device from build ==="
sed -i '/sde\/clone_cooling_device.o \\/d' techpack/display/msm/Makefile
grep -c "clone_cooling_device" techpack/display/msm/Makefile || true

echo "=== 3. any other users of clone members? ==="
grep -rln "brightness_clone\|get_by_type_a" techpack/ drivers/ 2>/dev/null | head -4

echo "=== 4. build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p7b-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|Error [0-9]|No rule" $BASE/logs/p7b-make.log | head -8

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7B_DONE
