#!/bin/bash
# p11b: fix replay gaps (pll_trace path, sde_drm.h), resume build
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p11b.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1
# pll_trace.h: original '.' path form (fresh tree)
sed -i 's|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH /home/smith/kernel-van/techpack/display/pll|' techpack/display/pll/pll_trace.h
grep -n "TRACE_INCLUDE_PATH" techpack/display/pll/pll_trace.h | head -2
# sde_drm.h from LOS 20 (FOD macros)
cp /home/smith/kernel-200/include/uapi/drm/sde_drm.h include/uapi/drm/sde_drm.h && echo sde_drm_copied
grep -c "FOD_PRESSED_LAYER_ZORDER" include/uapi/drm/sde_drm.h

echo "=== resume build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p11b-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|No rule" $BASE/logs/p11b-make.log | head -8

if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v11
  echo BUILD_OK_WITH_FORENSICS
else
  echo BUILD_FAILED
fi
echo P11B_DONE
