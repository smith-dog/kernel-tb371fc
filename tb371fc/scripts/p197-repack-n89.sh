#!/bin/bash
# p197-repack-n89.sh — repack Image-v27n89 into pure boot (Manager patch base).
# Same recipe as p185 (n88): apatch base + Image + dtb tail, no ramdisk swap.
BASE=/mnt/d/work/code-work/project/devices/tb371fc
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
cd $BASE || { echo NO_BASE; exit 1; }
md5sum $KV/arch/arm64/boot/Image
python3 tools/repack_boot.py firmware/apatch_patched_11266_0.13.5_clte.img \
    $KV/arch/arm64/boot/Image \
    out/boot-v27n89-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
echo "=== md5 ==="
md5sum out/boot-v27n89-pure-q706.img
