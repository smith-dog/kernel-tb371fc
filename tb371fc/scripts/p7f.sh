#!/bin/bash
# v7f: port mipi_dsi_dcs_set_display_brightness_big_endian into vanilla drm core
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7f.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
grep -n "mipi_dsi_dcs_set_display_brightness_big_endian" $K2/drivers/gpu/drm/drm_mipi_dsi.c | head -3
# extract function: from its definition line to the closing "^}" line
START=$(grep -n "^int mipi_dsi_dcs_set_display_brightness_big_endian" $K2/drivers/gpu/drm/drm_mipi_dsi.c | head -1 | cut -d: -f1)
if [ -z "$START" ]; then
  START=$(grep -n "mipi_dsi_dcs_set_display_brightness_big_endian" $K2/drivers/gpu/drm/drm_mipi_dsi.c | head -1 | cut -d: -f1)
fi
echo "func starts at line $START"
END=$(awk -v s=$START 'NR>s && /^}$/ {print NR; exit}' $K2/drivers/gpu/drm/drm_mipi_dsi.c)
echo "func ends at line $END"
sed -n "${START},${END}p" $K2/drivers/gpu/drm/drm_mipi_dsi.c > /tmp/func.txt
cat /tmp/func.txt
# append to vanilla (guard against double-append)
if ! grep -q "mipi_dsi_dcs_set_display_brightness_big_endian" $KV/drivers/gpu/drm/drm_mipi_dsi.c; then
  cat /tmp/func.txt >> $KV/drivers/gpu/drm/drm_mipi_dsi.c
  echo appended
fi
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7f-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7f-make.log | head -6
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image /mnt/d/work/code-work/project/tb371fc-kernel/out/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo V7F_DONE
