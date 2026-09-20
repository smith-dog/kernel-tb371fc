#!/bin/bash
# p138 build: v27n67 = p133+p134 gesture fixes + p137 pstore dcache flush
# + ramoops cmdline (WB mem_type, record_size included). Pure kernel, NO root.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p138-build.log 2>&1
echo "=== P138 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image 2>&1 | tail -8
MK=${PIPESTATUS[0]}
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo "BUILD FAILED"; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n67
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n67 out/boot-v27n67-q706.img out/dtbs/tail-new.bin "" "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" || { echo REPACK_FAILED; exit 1; }
echo "=== artifacts ==="
ls -la out/Image-v27n67 out/boot-v27n67-q706.img
echo "=== md5 ==="
md5sum out/Image-v27n67 out/boot-v27n67-q706.img
echo "=== P138 BUILD DONE $(date) ==="
