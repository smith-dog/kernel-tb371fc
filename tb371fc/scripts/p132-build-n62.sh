#!/bin/bash
# p132 build: debug-probe cleanup kernel v27n62 (TASK-021)
# Tree changes by p132-remove-probes.py: V27TRACE/BLFIX/p100 probe/P108 prints
# removed; tb_bb_* blackbox dead code removed (printk.c/setup.c/init main.c).
# Config unchanged vs n61 (#81). Product: pure kernel, NO root.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p132-build.log 2>&1
echo "=== P132 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image 2>&1 | tail -15
MK=${PIPESTATUS[0]}
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo "BUILD FAILED"; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n62
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n62 out/boot-v27n62-q706.img out/dtbs/tail-new.bin || { echo REPACK_FAILED; exit 1; }
echo "=== artifacts ==="
ls -la out/Image-v27n62 out/boot-v27n62-q706.img
echo "=== md5 ==="
md5sum out/Image-v27n62 out/boot-v27n62-q706.img
echo "=== strings sanity: removed prints must be absent ==="
if strings out/Image-v27n62 | grep -qE "V27TRACE|BLFIX|P100:|P108:"; then echo "CLEAN_FAILED: probe strings still in Image"; exit 1; else echo "IMAGE_CLEAN: no probe strings"; fi
if strings out/Image-v27n62 | grep -q "tb371fc-bb"; then echo "BLACKBOX_STILL_PRESENT"; exit 1; else echo "BLACKBOX_GONE"; fi
echo "=== P132 BUILD DONE $(date) ==="
