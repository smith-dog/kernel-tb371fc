#!/bin/bash
# p32 — v20-q706: 移植 vanilla techpack/display 进 paladin 树 + trace 修复 + 构建打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p32-display-port.log 2>&1
echo "=== P32 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
VAN=/home/smith/kernel-van
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== 1. copy techpack/display from vanilla ==="
if [ ! -d techpack/display ]; then
  cp -a $VAN/techpack/display techpack/display || { echo COPY_FAIL; exit 1; }
fi
ls techpack/display/ | head -8
echo "ARCH_KONA gate in display Makefile:"
grep -n "ARCH_KONA" techpack/display/Makefile | head -3

echo "=== 2. trace-path absolutize for new display tree ==="
grep -rl 'define TRACE_INCLUDE_PATH' --include='*.h' techpack/display 2>/dev/null | while read f; do
  val=$(grep -m1 'define TRACE_INCLUDE_PATH' "$f" | sed 's/.*define TRACE_INCLUDE_PATH //')
  case "$val" in
    $KV/*|/\$*) ;;
    *) sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $KV/$val|" "$f"; echo "abs: $f";;
  esac
done
# 自指误伤修复（同 p30g）
grep -rln 'define TRACE_INCLUDE_PATH' --include='*.h' techpack/display 2>/dev/null | while read f; do
  if grep 'define TRACE_INCLUDE_PATH' "$f" | grep -qE '\./|\.\.'; then
    d=$(dirname "$f")
    sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $KV/$d|" "$f"
    echo "selffix: $f"
  fi
done

echo "=== 3. build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p32-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target" $BASE/logs/p32-make.log | head -10
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
echo "=== P32 DONE $(date) ==="
