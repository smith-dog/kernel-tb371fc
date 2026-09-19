#!/bin/bash
# Build lineage-18.1 kernel: stock-close base + namespaces + pstore + PS5169
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-build181.log 2>&1
echo START
KDIR=/home/smith/kernel-181
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out

cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:$PATH

# 18.1's older arm64 Makefile feeds .S files to host `as`; force aarch64 GNU as
mkdir -p /usr/local/arm-as
ln -sf /usr/bin/aarch64-linux-gnu-as /usr/local/arm-as/as
export PATH=/usr/local/arm-as:$PATH
as --version | head -1

# local build fixes (tolerant across branches)
grep -q "CFLAGS_clk-debug" drivers/clk/qcom/Makefile 2>/dev/null || echo 'CFLAGS_clk-debug.o := -I$(src)' >> drivers/clk/qcom/Makefile
[ -f techpack/camera/config/konacamera.conf ] && sed -i 's/export CONFIG_SPECTRA_CAMERA=y/export CONFIG_SPECTRA_CAMERA=n/' techpack/camera/config/konacamera.conf
grep -q 'srctree)/\$(src)' techpack/display/pll/Makefile 2>/dev/null || echo 'ccflags-y += -I$(srctree)/$(src)' >> techpack/display/pll/Makefile

DEF=$(ls arch/arm64/configs/vendor/kona-perf_defconfig arch/arm64/configs/kona-perf_defconfig 2>/dev/null | head -1)
echo "defconfig: $DEF"
[ -n "$DEF" ] || { echo NO_DEFCONFIG; exit 1; }
cp $DEF .config

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
# the mission-critical additions
./scripts/config -e PS5169 -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE \
  -e PID_NS -e IPC_NS -e USER_NS -e CGROUP_NS -e KPROBES
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
echo "=== mission options ==="
grep -E "^CONFIG_(PS5169|PSTORE=|PSTORE_RAM|PID_NS|IPC_NS|USER_NS|CGROUP_NS|KPROBES|ARCH_QCOM)" .config

echo "=== build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p2-make181.log 2>&1
echo "make exit=$?"
grep -nE "error:" $BASE/logs/p2-make181.log | head -6

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-181
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo SCRIPT_END
