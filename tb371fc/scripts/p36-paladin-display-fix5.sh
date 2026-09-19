#!/bin/bash
# p36 — v20 修复5: backlight.c 同步 + touch 强制重建 + 重建打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p36-fix5.log 2>&1
echo "=== P36 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
VAN=/home/smith/kernel-van
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== 1. sync backlight.c ==="
cp -a $VAN/drivers/video/backlight/backlight.c drivers/video/backlight/backlight.c
grep -c "backlight_device_get_by_type_a" drivers/video/backlight/backlight.c

echo "=== 2. force rebuild of touched units ==="
touch drivers/gpu/drm/drm_mipi_dsi.c drivers/gpu/drm/drm_bridge.c \
      drivers/gpu/drm/drm_notifier_mi.c drivers/video/backlight/backlight.c
rm -f drivers/gpu/drm/drm_mipi_dsi.o drivers/gpu/drm/drm_bridge.o \
      drivers/gpu/drm/drm_notifier_mi.o drivers/video/backlight/backlight.o

echo "=== 3. rebuild ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p36-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target" $BASE/logs/p36-make.log | head -10
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v20q706
ls -la $BASE/out/Image-v20q706

echo "=== 4. repack boot ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v20q706 $BASE/out/boot-v20-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v20-q706.img
md5sum $BASE/out/boot-v20-q706.img
echo "=== P36 DONE $(date) ==="
