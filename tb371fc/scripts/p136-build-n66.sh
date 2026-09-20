#!/bin/bash
# p135 build: v27n66 = n64 (p133+p134 gesture fixes) + ramoops cmdline
# forensics (panic console persists across reboot at the DTB-reserved
# ramoops@27e000000 region, 2MB no-map; sizes mirror the DT node).
# Product: pure kernel, NO root.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p136-build.log 2>&1
echo "=== P136 BUILD START $(date) ==="
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
cp arch/arm64/boot/Image $BASE/out/Image-v27n66
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n66 out/boot-v27n66-q706.img out/dtbs/tail-new.bin "" "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.mem_type=1" || { echo REPACK_FAILED; exit 1; }
echo "=== artifacts ==="
ls -la out/Image-v27n66 out/boot-v27n66-q706.img
echo "=== md5 ==="
md5sum out/Image-v27n66 out/boot-v27n66-q706.img
echo "=== P136 BUILD DONE $(date) ==="
