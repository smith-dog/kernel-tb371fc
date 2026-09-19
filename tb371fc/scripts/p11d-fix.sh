#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p11d.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
# port mipi_dsi_dcs_set_display_brightness_big_endian from LOS 20 drm core (guarded)
if ! grep -q "mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c; then
  START=$(grep -n "^int mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c | head -1 | cut -d: -f1)
  [ -n "$START" ] || START=$(grep -n "mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c | head -1 | cut -d: -f1)
  END=$(awk -v s=$START 'NR>s && /^}$/ {print NR; exit}' drivers/gpu/drm/drm_mipi_dsi.c)
  echo "porting func lines $START-$END"
  sed -n "${START},${END}p" drivers/gpu/drm/drm_mipi_dsi.c >> drivers/gpu/drm/drm_mipi_dsi.c
fi
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p11d-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|No rule" $BASE/logs/p11d-make.log | head -8
if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v11
  echo BUILD_OK_WITH_FORENSICS
else
  echo BUILD_FAILED
fi
echo P11D_DONE
