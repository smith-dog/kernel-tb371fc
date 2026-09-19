#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-rtic3.log 2>&1
cd /home/smith/kernel-van || exit 1
echo "=== __rticdata definition ==="
grep -rn "define __rticdata" include/ arch/ drivers/ security/ 2>/dev/null | head -3
echo "=== all users ==="
grep -rln "__rticdata" --include="*.c" . 2>/dev/null | grep -v "\.git" | head -8
echo "=== patch all users ==="
for f in $(grep -rln "__rticdata" --include="*.c" . 2>/dev/null | grep -v "\.git"); do
  sed -i 's/ __rticdata;/;/' $f
  echo "patched: $f"
done
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|Error [0-9]" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log | head -6
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo RESUME3_DONE
