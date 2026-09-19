#!/bin/bash
# p117 build: integrate CLO audio-kernel techpack -> modules for audio bring-up
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p117-build.log 2>&1
echo "=== P117 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang      CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error      Image 2>&1 | tail -30
MK=${PIPESTATUS[0]}
echo "make exit=$MK"
echo '=== audio modules built ==='
find techpack/audio -name '*.ko' | sort
echo "=== P117 BUILD DONE $(date) ==="
