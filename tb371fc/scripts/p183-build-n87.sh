#!/bin/bash
# p183 build: v27n87 = n86 + CONFIG_SYSVIPC=y (Steam ARM64 needs SysV semaphores).
# Repacked with the manager-patched ramdisk (out/ramdisk-114142.img) so the
# image is root-self-contained, same flow as n68..n86.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p183-build.log 2>&1
echo "=== P183 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
grep -E '^CONFIG_SYSVIPC' .config
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > /tmp/p183-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; grep -nE "error:" /tmp/p183-make.log | head -10; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n87
cd $BASE
[ -s out/ramdisk-114142.img ] || { echo NO_RAMDISK; exit 1; }
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n87 out/boot-v27n87-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
echo "=== md5 ==="
md5sum out/Image-v27n87 out/boot-v27n87-patched-q706.img
echo "=== P183 BUILD DONE $(date) ==="
