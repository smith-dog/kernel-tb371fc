#!/bin/bash
# p364 — TASK-044: ntsync 移植版构建(main HEAD + p273/p273b/p273c 适配)
# 产物: out/Image-ntsync1 → out/boot-ntsync1-kspatched-flash.img(基=boot-v1.7,kernel swap)
exec > /mnt/d/work/code-work/project/devices/tb371fc/logs/p364-build-ntsync.log 2>&1
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
BASE=/mnt/d/work/code-work/project/devices/tb371fc
echo "=== P364 NTSYNC BUILD START $(date) ==="
set -e
cd $KV
git branch --show-current
git log --oneline -3 | cat
grep '^CONFIG_NTSYNC' .config

run_make() {
  make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
       CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
       KCFLAGS="-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time -include $KV/drivers/staging/fw-api/fw/p226_compat.h" \
       DYNAMIC_SINGLE_CHIP=qca6390 "$@"
}

echo "=== Image ==="
run_make Image
echo "=== modules ==="
run_make modules

cp arch/arm64/boot/Image $BASE/out/Image-ntsync1
echo "=== ksu.ko 校验(必须与 v1.7 ramdisk 一致: 8d5a1e1b560802977456a924cc7dd213) ==="
KSUMD5=$(find . -name 'ksu.ko' -not -path './staging/*' | head -1 | xargs md5sum | cut -d' ' -f1)
echo "ksu.ko md5 = $KSUMD5"
if [ "$KSUMD5" != "8d5a1e1b560802977456a924cc7dd213" ]; then
  echo "FATAL: ksu.ko changed — ramdisk swap needed, repack aborted"
  exit 9
fi

echo "=== repack(基=boot-v1.7-kspatched-flash.img,ramdisk/DTB tail 原样保留) ==="
cd $BASE
python3 $KV/tb371fc/tools/repack_boot.py \
    out/boot-v1.7-kspatched-flash.img \
    out/Image-ntsync1 \
    out/boot-ntsync1-kspatched-flash.img 2>&1 | tail -3

echo "=== banner ==="
strings out/Image-ntsync1 | grep -m1 "Linux version"
echo "ntsync strings count:"
strings out/Image-ntsync1 | grep -c "ntsync" || true

echo "=== md5 ==="
md5sum out/Image-ntsync1 out/boot-ntsync1-kspatched-flash.img
echo "=== P364 NTSYNC BUILD DONE $(date) ==="
