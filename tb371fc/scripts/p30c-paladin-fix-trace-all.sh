#!/bin/bash
# p30c — 树级 trace include 修复：TRACE_INCLUDE_PATH . -> 绝对路径（clang 兼容）
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p30c-fixtrace.log 2>&1
echo "=== P30C START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== fix all TRACE_INCLUDE_PATH '.' headers ==="
N=0
grep -rl 'define TRACE_INCLUDE_PATH \.' --include='*.h' drivers sound net kernel include arch/arm64 2>/dev/null | while read f; do
  d=$(dirname "$f")
  sed -i "s|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH $d|" "$f"
  echo "fixed: $f"
done

echo "=== resume build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p30c-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|No rule to make target" $BASE/logs/p30c-make.log | head -8
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v19q706
ls -la $BASE/out/Image-v19q706

echo "=== repack boot ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v19q706 $BASE/out/boot-v19-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v19-q706.img
md5sum $BASE/out/boot-v19-q706.img
echo "=== P30C DONE $(date) ==="
