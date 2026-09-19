#!/bin/bash
# p37b — v21 增量重建（ipa 守卫修复后）+ 打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p37b-make2.log 2>&1
echo "=== P37B START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/local/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin:$PATH
cd $KV || exit 1

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- CC=aarch64-none-elf-gcc KCFLAGS=-Wno-error Image
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "error:" $BASE/logs/p37b-make.log 2>/dev/null | head -5
  grep -nE "error:" $BASE/logs/p37b-make2.log 2>/dev/null | head -5
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v21q706
ls -la $BASE/out/Image-v21q706
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v21q706 $BASE/out/boot-v21-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v21-q706.img
md5sum $BASE/out/boot-v21-q706.img
echo "=== P37B DONE $(date) ==="
