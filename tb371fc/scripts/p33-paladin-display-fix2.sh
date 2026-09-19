#!/bin/bash
# p33 — v20 修复: sde_drm.h(uapi) 同步 + display trace 头自指归位 + 重建打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p33-fix2.log 2>&1
echo "=== P33 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
VAN=/home/smith/kernel-van
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== 1. sync sde_drm.h from vanilla (FOD define etc.) ==="
cp -a $VAN/include/uapi/drm/sde_drm.h include/uapi/drm/sde_drm.h
grep -n "FOD_PRESSED_LAYER_ZORDER" include/uapi/drm/sde_drm.h | head -2

echo "=== 2. all display trace headers -> own dir ==="
grep -rl 'define TRACE_INCLUDE_PATH' --include='*.h' techpack/display 2>/dev/null | while read f; do
  d=$(dirname "$f")
  cur=$(grep -m1 'define TRACE_INCLUDE_PATH' "$f" | sed 's/.*define TRACE_INCLUDE_PATH //')
  if [ "$cur" != "$KV/$d" ]; then
    sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $KV/$d|" "$f"
    echo "selfdir: $f"
  fi
done

echo "=== 3. rebuild ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p33-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target" $BASE/logs/p33-make.log | head -10
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
echo "=== P33 DONE $(date) ==="
