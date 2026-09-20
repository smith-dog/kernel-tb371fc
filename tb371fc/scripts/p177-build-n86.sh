#!/bin/bash
# p177 build: v27n86 = single reset per wake cycle (p170: gesture-IRQ reset
# for double-tap wakes, deferred reset only for non-gesture wakes).
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p177-build.log 2>&1
echo "=== P177 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
cd $KV
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /tmp/p177-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then
  echo "--- failure ---"
  grep -nE "error:" /tmp/p177-make.log | head -10
  exit 1
fi
echo "markers: gesture_id=$(strings arch/arm64/boot/Image | grep -c 'gesture_id = ') wake-reset-applied=$(strings arch/arm64/boot/Image | grep -c 'wake reset applied')"
cp arch/arm64/boot/Image $BASE/out/Image-v27n86
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n86 out/boot-v27n86-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
md5sum out/Image-v27n86 out/boot-v27n86-patched-q706.img
echo "=== P177 DONE $(date) ==="
