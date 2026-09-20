#!/bin/bash
# p161 build: v27n81 = n77 + p160 (exit gesture mode before wake key report)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p167-build.log 2>&1
echo "=== P167 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
cd $KV
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /tmp/p161-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then
  echo "--- failure ---"
  grep -nE "error:" /tmp/p161-make.log | head -10
  exit 1
fi
echo "Image gesture exit marker: $(strings arch/arm64/boot/Image | grep -c 'gesture_id = ')"
cp arch/arm64/boot/Image $BASE/out/Image-v27n81
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n81 out/boot-v27n81-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
md5sum out/Image-v27n81 out/boot-v27n81-patched-q706.img
echo "=== P167 DONE $(date) ==="
