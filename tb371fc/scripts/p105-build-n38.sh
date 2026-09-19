#!/bin/bash
# p105 build: incremental Image with p105 (p93 guard fix) + repack boot-v27n38
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p105-build.log 2>&1
echo "=== P105 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image
MK=$?
echo "make exit=$MK"
[ $MK -ne 0 ] && { echo BUILD_FAILED; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v27n38
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v27n38 $BASE/out/boot-v27n38-q706.img $BASE/out/dtbs/tail-new.bin
echo "repack exit=$?"
md5sum $BASE/out/boot-v27n38-q706.img
echo "=== P105 BUILD DONE $(date) ==="
