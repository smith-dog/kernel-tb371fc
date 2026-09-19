#!/bin/bash
# p34 — v20 修复3: backlight/drm 头文件同步 + mipi_dsi 大端亮度函数 + 重建
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p34-fix3.log 2>&1
echo "=== P34 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
VAN=/home/smith/kernel-van
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== 1. sync headers from vanilla ==="
cp -a $VAN/include/linux/backlight.h include/linux/backlight.h
cp -a $VAN/include/drm/drm_bridge.h include/drm/drm_bridge.h
cp -a $VAN/include/drm/drm_mipi_dsi.h include/drm/drm_mipi_dsi.h
grep -c "thermal_brightness_clone_limit" include/linux/backlight.h

echo "=== 2. add big-endian brightness fn to drm_mipi_dsi.c ==="
if ! grep -q "mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c; then
  python3 - <<'PYEOF'
src = open('/home/smith/kernel-van/drivers/gpu/drm/drm_mipi_dsi.c', encoding='utf-8').read()
i = src.find('int mipi_dsi_dcs_set_display_brightness_big_endian')
assert i > 0, 'fn not found in van'
# 找函数结束（从 i 起第一个单独的 '}' 行）
j = src.find('\n}\n', i)
assert j > 0
fn = src[i:j+3]
dst = open('drivers/gpu/drm/drm_mipi_dsi.c', encoding='utf-8').read()
open('drivers/gpu/drm/drm_mipi_dsi.c', 'a', encoding='utf-8').write('\n' + fn + '\n')
print('appended', len(fn), 'bytes')
PYEOF
fi
grep -n "mipi_dsi_dcs_set_display_brightness_big_endian" drivers/gpu/drm/drm_mipi_dsi.c | head -2

echo "=== 3. rebuild ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p34-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target" $BASE/logs/p34-make.log | head -10
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
echo "=== P34 DONE $(date) ==="
