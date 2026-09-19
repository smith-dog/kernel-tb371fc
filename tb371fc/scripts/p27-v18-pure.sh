#!/bin/bash
# v18: PURE minimal vanilla build - kona-perf_defconfig only (no display port,
# no docker set, no fixes). Blackbox code stays (printk.c). Mini-ramdisk boot:
# if the kernel lives, mini-init navigates to fastboot autonomously.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p27-v18.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
cd $KV || exit 1

echo "=== save docker config, switch to pure defconfig ==="
cp .config $BASE/out/config-docker-backup
DEF=$(ls arch/arm64/configs/vendor/kona-perf_defconfig arch/arm64/configs/kona-perf_defconfig 2>/dev/null | head -1)
echo "defconfig: $DEF"
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig >/dev/null 2>&1
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- $DEF >/dev/null 2>&1
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig >/dev/null 2>&1
grep -cE "TECHPACK|DRM_MSM" .config

echo "=== full rebuild (may take a while) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p27-make.log 2>&1
echo "make exit=$?"
grep -E "error:" $BASE/logs/p27-make.log | head -4
[ -f arch/arm64/boot/Image ] || { echo BUILD_FAILED; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v18
ls -la $BASE/out/Image-v18

echo "=== repack with mini-ramdisk ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v18 $BASE/out/boot-v18.img $BASE/out/dtbs/tail-new.bin $BASE/out/mini-ramdisk2b.img
ls -la $BASE/out/boot-v18.img
echo V18_DONE
