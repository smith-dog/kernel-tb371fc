#!/bin/bash
# p35 — v20 修复4: drm_mipi_dsi/bridge .c+h 整体同步 + drm_notifier_mi 移植 + 重建
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p35-fix4.log 2>&1
echo "=== P35 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
VAN=/home/smith/kernel-van
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== 1. replace drm_mipi_dsi/bridge (.c+.h) with vanilla versions ==="
cp -a $VAN/drivers/gpu/drm/drm_mipi_dsi.c drivers/gpu/drm/drm_mipi_dsi.c
cp -a $VAN/drivers/gpu/drm/drm_bridge.c   drivers/gpu/drm/drm_bridge.c
grep -c "set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c

echo "=== 2. port drm_notifier_mi (Xiaomi notifier, display tree dependency) ==="
cp -a $VAN/include/drm/drm_notifier_mi.h include/drm/drm_notifier_mi.h
cp -a $VAN/drivers/gpu/drm/drm_notifier_mi.c drivers/gpu/drm/drm_notifier_mi.c
grep -q "drm_notifier_mi.o" drivers/gpu/drm/Makefile || echo 'obj-y += drm_notifier_mi.o' >> drivers/gpu/drm/Makefile
tail -2 drivers/gpu/drm/Makefile

echo "=== 3. rebuild ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p35-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target" $BASE/logs/p35-make.log | head -10
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
echo "=== P35 DONE $(date) ==="
