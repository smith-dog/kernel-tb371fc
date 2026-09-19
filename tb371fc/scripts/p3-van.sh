#!/bin/bash
# Build vanilla QC msm-4.19 (LA.UM.9.12.c25): namespaces + pstore + PS5169
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-van.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
KDIR=/home/smith/kernel-van

echo "=== rebuild worktree from .git ==="
rm -rf $KDIR
mkdir -p $KDIR
cp -a $BASE/reference/msm-4.19-van/.git $KDIR/.git
cd $KDIR || exit 1
git config --global --add safe.directory $KDIR
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -1
head -5 Makefile
echo "CR in Makefile: $(grep -c $'\r' Makefile || true)"
git log --oneline -1
ls arch/arm64/configs/vendor/ 2>/dev/null | grep kona

export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

echo "=== apply local fixes (tolerant) ==="
grep -q "CFLAGS_clk-debug" drivers/clk/qcom/Makefile 2>/dev/null || echo 'CFLAGS_clk-debug.o := -I$(src)' >> drivers/clk/qcom/Makefile
[ -f techpack/camera/config/konacamera.conf ] && sed -i 's/export CONFIG_SPECTRA_CAMERA=y/export CONFIG_SPECTRA_CAMERA=n/' techpack/camera/config/konacamera.conf
[ -f techpack/display/pll/Makefile ] && grep -q 'srctree)/\$(src)' techpack/display/pll/Makefile 2>/dev/null || echo 'ccflags-y += -I$(srctree)/$(src)' >> techpack/display/pll/Makefile

DEF=$(ls arch/arm64/configs/vendor/kona-perf_defconfig arch/arm64/configs/kona-perf_defconfig 2>/dev/null | head -1)
echo "defconfig: $DEF"
[ -n "$DEF" ] || { echo NO_DEFCONFIG; exit 1; }
cp $DEF .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
./scripts/config -e PS5169 -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PID_NS -e IPC_NS -e USER_NS -e CGROUP_NS -e KPROBES
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
echo "=== mission options ==="
grep -E "^CONFIG_(PS5169|PSTORE=|PSTORE_RAM|PID_NS|IPC_NS|USER_NS|CGROUP_NS|KPROBES|ARCH_QCOM)" .config

echo "=== build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p3-make.log 2>&1
echo "make exit=$?"
grep -nE "error:" $BASE/logs/p3-make.log | head -8

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-van
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo SCRIPT_END
