#!/bin/bash
# p191-build-n89.sh — v27n89 = n88 + p190 (auto OTG: sink attach -> host).
# p189 (trace INCLUDE paths) fixed the tree-move breakage; p187 reverted.
# .config untouched => MODVERSIONS CRC stable; ksu.ko rebuilt in-tree.
BASE=/mnt/d/work/code-work/project/devices/tb371fc
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7
LOG=$BASE/logs/p191-build.log
exec > "$LOG" 2>&1
echo "=== P191 BUILD START $(date) ==="
export PATH=$SNAP/bin:$PATH
cd "$KV" || { echo NO_TREE; exit 1; }

echo "--- patch state ---"
grep -n "drop Lenovo's otg_state gate" drivers/usb/pd/policy_engine.c || { echo P190_MISSING; exit 1; }
grep -E '^CONFIG_SYSVIPC=|^# CONFIG_SYSVIPC' .config

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image
MK=$?
echo "make Image exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; grep -nE "error:" /tmp/p191-make.log 2>/dev/null; grep -nE "error:" "$LOG" | head -10; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n89

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     modules
MK=$?
echo "make modules exit=$MK"
if [ "$MK" != "0" ]; then echo MODULES_FAILED; exit 1; fi

mkdir -p $BASE/out/ko-v27n89
find . -name '*.ko' -newer .config -exec cp {} $BASE/out/ko-v27n89/ \; 2>/dev/null
ls $BASE/out/ko-v27n89 | wc -l > $BASE/out/ko-v27n89/.count

cd "$BASE"
echo "=== banner ==="
strings out/Image-v27n89 | grep -m1 "Linux version"
echo "=== ksu.ko ==="
ls -la out/ko-v27n89/ksu.ko 2>/dev/null || echo NO_KSU_KO
echo "=== md5 ==="
md5sum out/Image-v27n89
echo "=== P191 BUILD DONE $(date) ==="
