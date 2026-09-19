#!/bin/bash
# v10: causal isolation — v9 WITHOUT QCOM_MINIDUMP (keep display+forensic timer)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p10-van.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1
./scripts/config -d QCOM_MINIDUMP
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "QCOM_MINIDUMP" .config | head -2

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p10-make.log 2>&1
echo "make exit=$?"
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-v10
  echo BUILD_OK
else
  echo BUILD_FAILED
fi

echo "=== repack boot-v10.img ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v10 $OUT/boot-v10.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v10.img
echo V10_DONE
