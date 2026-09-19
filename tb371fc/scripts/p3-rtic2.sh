#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-rtic2.log 2>&1
cd /home/smith/kernel-van || exit 1
grep -E "RTIC" .config | head -3
./scripts/config -d QCOM_RTIC -d RTIC -d MODVERSIONS 2>/dev/null
grep -E "CONFIG_RTIC|CONFIG_QCOM_RTIC|CONFIG_MODVERSIONS" .config | head -3
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "CONFIG_QCOM_RTIC|CONFIG_MODVERSIONS" .config | head -3
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|No rule" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-make.log | head -6
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo RESUME2_DONE
