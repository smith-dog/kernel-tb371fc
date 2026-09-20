#!/bin/bash
# p169 build: v27n82 = n79-shape gesture path with all naked resets removed
# (p167 touch: revert p160 + p165 reset, keep p133/p142/p146/p149/p154;
#  p168 display: dsi_display.c back to p140 v2 shape, UNBLANK at enable end)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p169-build.log 2>&1
echo "=== P169 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
cd $KV
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /tmp/p169-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then
  echo "--- failure ---"
  grep -nE "error:" /tmp/p169-make.log | head -10
  exit 1
fi
echo "Image gesture marker: $(strings arch/arm64/boot/Image | grep -c 'gesture_id = ')"
echo "Image p167 marker:    $(strings arch/arm64/boot/Image | grep -c 'TB371FC p167')"
echo "Image p160 gone:      $(strings arch/arm64/boot/Image | grep -c 'TB371FC p160')"
cp arch/arm64/boot/Image $BASE/out/Image-v27n82
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n82 out/boot-v27n82-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
md5sum out/Image-v27n82 out/boot-v27n82-patched-q706.img
echo "=== P169 DONE $(date) ==="
