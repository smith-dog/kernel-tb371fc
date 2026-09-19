#!/bin/bash
# p30f — TRACE_INCLUDE_PATH 绝对化（完整版，无 head 截断）+ ipa 误伤修复 + 续编打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p30f-fixtrace4.log 2>&1
echo "=== P30F START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== fix ipa mangled path ==="
sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $KV/drivers/platform/msm/ipa/ipa_v3|" drivers/platform/msm/ipa/ipa_v3/ipa_trace.h
grep -n 'TRACE_INCLUDE_PATH' drivers/platform/msm/ipa/ipa_v3/ipa_trace.h

echo "=== absolutize ALL remaining relative TRACE_INCLUDE_PATH (no truncation) ==="
N=0
grep -rl 'define TRACE_INCLUDE_PATH' --include='*.h' drivers sound net kernel include mm fs crypto block security lib arch/arm64 2>/dev/null > /tmp/tracepaths.txt
wc -l /tmp/tracepaths.txt
while read f; do
  val=$(grep -m1 'define TRACE_INCLUDE_PATH' "$f" | sed 's/.*define TRACE_INCLUDE_PATH //')
  case "$val" in
    $KV/*|/\$*) ;;                       # 已绝对化，跳过
    "") ;;
    *) sed -i "s|^#define TRACE_INCLUDE_PATH .*|#define TRACE_INCLUDE_PATH $KV/$val|" "$f"; N=$((N+1)); echo "abs($N): $f";;
  esac
done < /tmp/tracepaths.txt
echo "total newly absolutized: $N"

echo "=== resume build ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p30f-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|No rule to make target" $BASE/logs/p30f-make.log | head -8
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
echo "=== P30F DONE $(date) ==="
