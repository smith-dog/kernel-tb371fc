#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p11e.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
if grep -q "mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c; then
  echo "already ported"
else
  START=$(grep -n "^int mipi_dsi_dcs_set_display_brightness_big_endian" $K2/drivers/gpu/drm/drm_mipi_dsi.c | head -1 | cut -d: -f1)
  echo "source def at line $START"
  END=$(awk -v s=$START 'NR>s && /^}$/ {print NR; exit}' $K2/drivers/gpu/drm/drm_mipi_dsi.c)
  echo "ends at $END"
  sed -n "${START},${END}p" $K2/drivers/gpu/drm/drm_mipi_dsi.c >> drivers/gpu/drm/drm_mipi_dsi.c
fi
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p11e-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference|No rule" $BASE/logs/p11e-make.log | head -8
if [ -f arch/arm64/boot/Image ] && grep -q "TB371FC_FORENSIC_DUMP_TRIGGER" arch/arm64/boot/Image; then
  cp arch/arm64/boot/Image $OUT/Image-v11
  echo BUILD_OK_WITH_FORENSICS
else
  echo BUILD_FAILED
fi
echo P11E_DONE
