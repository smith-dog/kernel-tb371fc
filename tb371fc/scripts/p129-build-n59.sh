#!/bin/bash
# p129 build: charge_disable attr 0666 -> v27n59 (#73)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p129-build.log 2>&1
echo "=== P129 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang      CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error      Image 2>&1 | tail -15
MK=${PIPESTATUS[0]}
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo "BUILD FAILED"; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n59
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n59 out/boot-v27n59-q706.img out/dtbs/tail-new.bin
echo "=== md5 ==="
md5sum out/boot-v27n59-q706.img
echo "=== P129 BUILD DONE $(date) ==="
