#!/bin/bash
# p324 — n96r: TASK-039 IMEM console ring (crash forensics) on top of #174
# change: + drivers/soc/qcom/t39_imem_console.c (obj-y)
# base for kspatched repack: boot-v27n96q-ramoops-kspatched-flash.img (ksu ramdisk + ramoops DTB tail verbatim)
exec > /mnt/d/work/code-work/project/devices/tb371fc/logs/p324-build.log 2>&1
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
BASE=/mnt/d/work/code-work/project/devices/tb371fc
echo "=== P324 BUILD START $(date) ==="
set -e
cd $KV
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS="-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time -include /home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/staging/fw-api/fw/p226_compat.h" \
     DYNAMIC_SINGLE_CHIP=qca6390 \
     Image
cp arch/arm64/boot/Image $BASE/out/Image-v27n96r
cd $BASE
python3 $KV/tb371fc/tools/repack_boot.py out/kernelsu_patched_20260924_084550.img \
    out/Image-v27n96r out/boot-v27n96r-kspatched-flash.img out/dtbs/tail-new.bin 2>&1 | tail -3
echo "=== banner ==="
strings out/Image-v27n96r | grep -m1 "Linux version"
strings out/Image-v27n96r | grep -c "t39-imem" || true
echo "=== md5 ==="
md5sum out/Image-v27n96r out/boot-v27n96r-kspatched-flash.img
echo "=== P324 BUILD DONE $(date) ==="
