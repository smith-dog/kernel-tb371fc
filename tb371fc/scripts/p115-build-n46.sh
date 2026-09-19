#!/bin/bash
# p115 build: module-compat config (SIG off, MODVERSIONS=y, LOCALVERSION=-perf+) + repack boot-v27n46
# NOTE: MODVERSIONS flips version generation -> near-full rebuild expected.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p115-build.log 2>&1
echo "=== P115 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
python3 $BASE/scripts/p115-module-compat.py || { echo PATCH_FAILED; exit 1; }
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     olddefconfig Image
MK=$?
echo "make exit=$MK"
[ $MK -ne 0 ] && { echo BUILD_FAILED; grep -nE 'fatal error|error:' $BASE/logs/p115-build.log | head -10; exit 1; }
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
echo "=== release check ==="
strings arch/arm64/boot/Image | grep -m2 'Linux version 4.19'
cp arch/arm64/boot/Image $BASE/out/Image-v27n46
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v27n46 $BASE/out/boot-v27n46-q706.img $BASE/out/dtbs/tail-new.bin
echo "repack exit=$?"
md5sum $BASE/out/boot-v27n46-q706.img
echo "=== P115 BUILD DONE $(date) ==="
