#!/bin/bash
# p268 — incremental build after p267 (kernel-side wlan self-boot trigger)
exec > /mnt/d/work/code-work/project/devices/tb371fc/logs/p268-build.log 2>&1
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
T34=/home/smith/t34
BASE=/mnt/d/work/code-work/project/devices/tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7
echo "=== P268 BUILD START $(date) ==="
set -e
cp $T34/qc22/core/hdd/src/wlan_hdd_main.c $KV/drivers/staging/qcacld-3.0/core/hdd/src/wlan_hdd_main.c
export PATH=$SNAP/bin:$PATH
cd $KV
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS="-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time -include /home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/staging/fw-api/fw/p226_compat.h" \
     DYNAMIC_SINGLE_CHIP=qca6390 \
     Image
cp arch/arm64/boot/Image $BASE/out/Image-v27n96c
cd $BASE
python3 tools/repack_boot.py firmware/apatch_patched_11266_0.13.5_clte.img \
    out/Image-v27n96c out/boot-v27n96c-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
python3 tools/repack_boot.py out/kernelsu_patched_20260924_084550.img \
    out/Image-v27n96c out/boot-v27n96c-kspatched-flash.img out/dtbs/tail-new.bin 2>&1 | tail -2
echo "=== banner ==="
strings out/Image-v27n96c | grep -m1 "Linux version"
echo "=== md5 ==="
md5sum out/Image-v27n96c out/boot-v27n96c-kspatched-flash.img
echo "=== P268 BUILD DONE $(date) ==="
