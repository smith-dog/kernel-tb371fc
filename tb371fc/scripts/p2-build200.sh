#!/bin/bash
# Build lineage-20 kernel: namespaces + pstore + PS5169 (single-instance!)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-build200.log 2>&1
echo START
KDIR=/home/smith/kernel-200
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out

cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:$PATH

# local build fixes
grep -q "CFLAGS_clk-debug" drivers/clk/qcom/Makefile 2>/dev/null || echo 'CFLAGS_clk-debug.o := -I$(src)' >> drivers/clk/qcom/Makefile
[ -f techpack/camera/config/konacamera.conf ] && sed -i 's/export CONFIG_SPECTRA_CAMERA=y/export CONFIG_SPECTRA_CAMERA=n/' techpack/camera/config/konacamera.conf
M=techpack/display/pll/Makefile
grep -q 'srctree)/\$(src)' $M 2>/dev/null || {
  sed -i 's|^\(ccflags-y[[:space:]]*[:+]*=[[:space:]]*.*\)$|\1 -I\$(srctree)/\$(src)|' $M
  grep -q 'srctree)/\$(src)' $M || sed -i '1i ccflags-y := -I\$(srctree)/\$(src)' $M
}

DEF=$(ls arch/arm64/configs/vendor/kona-perf_defconfig arch/arm64/configs/kona-perf_defconfig 2>/dev/null | head -1)
[ -n "$DEF" ] || { echo NO_DEFCONFIG; exit 1; }
cp $DEF .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
./scripts/config -e PS5169 -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PID_NS -e IPC_NS -e USER_NS -e CGROUP_NS -e KPROBES
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
echo "=== mission options ==="
grep -E "^CONFIG_(PS5169|PSTORE=|PSTORE_RAM|PID_NS|IPC_NS|USER_NS|CGROUP_NS|KPROBES|ARCH_QCOM)" .config

echo "=== build Image ==="
MLOG=$BASE/logs/p2-make200.log
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- KCFLAGS=-Wno-error Image > $MLOG 2>&1
echo "make exit=$?"
grep -nE "error:" $MLOG | head -6

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-200
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo SCRIPT_END
