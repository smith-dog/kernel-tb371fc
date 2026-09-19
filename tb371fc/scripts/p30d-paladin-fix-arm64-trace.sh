#!/bin/bash
# p30d — 修复 arm64 perf_trace 头的相对路径（sed 误伤），续编 + 打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p30d-fixtrace2.log 2>&1
echo "=== P30D START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== fix arm64 perf trace headers to absolute path ==="
GOOD="$KV/arch/arm64/kernel"
for f in arch/arm64/kernel/perf_trace_user.h arch/arm64/kernel/perf_trace_counters.h; do
  [ -f "$f" ] || continue
  sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $GOOD|" "$f"
  grep -n 'TRACE_INCLUDE_PATH' "$f"
done

echo "=== scan for other mangled/relative TRACE_INCLUDE_PATH in arm64-relevant dirs ==="
grep -rn 'define TRACE_INCLUDE_PATH' --include='*.h' arch/arm64 drivers sound net security lib mm fs crypto block 2>/dev/null | grep -v "$KV/" | grep -E '\.(\./|\.\./)|\.\./' | head -8
grep -rn "define TRACE_INCLUDE_PATH $KV" --include='*.h' drivers 2>/dev/null | grep -E '\./|\.\./' | head -5 || true

echo "=== resume build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p30d-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|No rule to make target" $BASE/logs/p30d-make.log | head -8
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
echo "=== P30D DONE $(date) ==="
