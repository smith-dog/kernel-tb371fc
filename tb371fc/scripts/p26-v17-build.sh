#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p26-build.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p26-make.log 2>&1
echo "make exit=$?"
grep -E "error:" $BASE/logs/p26-make.log | head -4
[ -f arch/arm64/boot/Image ] || { echo BUILD_FAILED; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v17
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v17 $BASE/out/boot-v17.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v17.img
echo V17_DONE
