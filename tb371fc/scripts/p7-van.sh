#!/bin/bash
# v7: port display stack (from LOS 23.2, proven buildable) into vanilla kernel, rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7-van.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KDIR=/home/smith/kernel-van
cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

echo "=== port display stack ==="
rm -rf techpack/display
cp -a /home/smith/kernel-200/techpack/display techpack/display
rm -f techpack/display/built-in.a
find techpack/display -name ".*.cmd" -delete
# re-point pll trace include to THIS tree
sed -i 's|define TRACE_INCLUDE_PATH /home/smith/kernel-200/techpack/display/pll|define TRACE_INCLUDE_PATH /home/smith/kernel-van/techpack/display/pll|' techpack/display/pll/pll_trace.h
grep -n "TRACE_INCLUDE_PATH" techpack/display/pll/pll_trace.h | head -2

echo "=== build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p7-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|Error [0-9]|No rule" $BASE/logs/p7-make.log | head -8

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7_DONE
