#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-fixhid.log 2>&1
cd /home/smith/kernel-van || exit 1
sed -i 's|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH /home/smith/kernel-van/drivers/hid|' drivers/hid/hid-trace.h
grep -n "TRACE_INCLUDE_PATH" drivers/hid/hid-trace.h | head -2
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|No rule to make target" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log | head -6
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo RESUME_DONE
