#!/bin/bash
# p188-build-n89.sh — v27n89 = v27n88 + p187 (smb5 VBUS regulator: use the PMIC's
# internal OTG boost when the DT provides no qcom,gpio_boost_en).
# Driver-only change: .config untouched => ksu.ko (32649) stays CRC-compatible.
BASE=/mnt/d/work/code-work/project/devices/tb371fc
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7
LOG=$BASE/logs/p188-build.log
exec > "$LOG" 2>&1
echo "=== P188 BUILD START $(date) ==="
export PATH=$SNAP/bin:$PATH
cd "$KV"

echo "--- patch state ---"
grep -n "gpio_is_valid(chg->gpio_boost_en)" drivers/power/supply/qcom/smb5-lib.c || { echo P187_MISSING; exit 1; }

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n89

cd "$BASE"
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n89 \
    out/boot-v27n89-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
if [ -s out/ramdisk-114142.img ]; then
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n89 \
    out/boot-v27n89-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img \
    "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
else
  echo "WARN_NO_RAMDISK (patched image skipped)"
fi
echo "=== banner ==="
strings out/Image-v27n89 | grep -m1 "Linux version"
echo "=== md5 ==="
md5sum out/Image-v27n89 out/boot-v27n89-*.img
echo "=== P188 BUILD DONE $(date) ==="
