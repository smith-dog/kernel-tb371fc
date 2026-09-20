#!/bin/bash
# p151 build: v27n74 = n73 + p150 gesture-wake light resume (no reflash)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p151-build.log 2>&1
echo "=== P151 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
cd $KV
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /tmp/p151-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then
  echo "--- failure details ---"
  grep -nE "error:" /tmp/p151-make.log | head -10
  exit 1
fi
echo "--- verify string in artifacts ---"
echo "obj nvt_wake_key: $(strings drivers/input/touchscreen/nt36532/nt36xxx.o | grep -c nvt_wake_key)"
echo "Image nvt_wake_key: $(strings arch/arm64/boot/Image | grep -c nvt_wake_key)"
cp arch/arm64/boot/Image $BASE/out/Image-v27n74
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n74 out/boot-v27n74-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
md5sum out/Image-v27n74 out/boot-v27n74-patched-q706.img
echo "=== P151 DONE $(date) ==="
