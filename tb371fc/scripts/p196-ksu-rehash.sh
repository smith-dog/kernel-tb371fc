#!/bin/bash
# p196-ksu-rehash.sh — rebuild ksu.ko with the new EXPECTED_SIZE/HASH and push-ready copy
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
BASE=/mnt/d/work/code-work/project/devices/tb371fc
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
cd "$KV" || { echo NO_TREE; exit 1; }
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     drivers/kernelsu/ksu.ko
RC=$?
echo "ksu.ko make exit=$RC"
[ $RC -ne 0 ] && exit 1
cp drivers/kernelsu/ksu.ko $BASE/out/ko-v27n89/ksu.ko
md5sum drivers/kernelsu/ksu.ko $BASE/out/ko-v27n89/ksu.ko
strings drivers/kernelsu/ksu.ko | grep -m1 6be65f23 && echo HASH_EMBEDDED_OK
