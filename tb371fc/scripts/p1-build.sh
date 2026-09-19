#!/bin/bash
# P1: build minimally-changed kernel (kona-perf base, no namespace changes), target Image
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-build.log 2>&1
echo START
KDIR=/home/smith/android_kernel_xiaomi_sm8250
OUTDIR=/mnt/d/work/code-work/project/tb371fc-kernel/out
mkdir -p $OUTDIR

# cross binutils + cpio (kheaders)
apt-get install -y binutils-aarch64-linux-gnu cpio >> /dev/null 2>&1
command -v aarch64-linux-gnu-ld || { echo "MISSING cross binutils"; exit 1; }

cd $KDIR || exit 1
git log --oneline -1

# local build fixes (documented; upstream LOS tree lacks these)
grep -q "CFLAGS_clk-debug" drivers/clk/qcom/Makefile || echo 'CFLAGS_clk-debug.o := -I$(src)' >> drivers/clk/qcom/Makefile
sed -i 's/export CONFIG_SPECTRA_CAMERA=y/export CONFIG_SPECTRA_CAMERA=n/' techpack/camera/config/konacamera.conf
sed -i '/ccflags-y += /d' techpack/display/pll/Makefile
sed -i 's|\(-I\$(srctree)/drivers/clk/qcom/\)$|\1 -I\$(srctree)/\$(src)|' techpack/display/pll/Makefile

export PATH=/usr/lib/llvm-17/bin:$PATH
clang --version | head -1

echo "=== base config ==="
cp arch/arm64/configs/vendor/kona-perf_defconfig .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
# TB371FC has a PS5169 USB redriver (stock: CONFIG_USB_REDRIVER_PS5169=y); LOS symbol: PS5169
./scripts/config -e PS5169
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig

echo "=== critical options after olddefconfig ==="
grep -E "^CONFIG_(ARCH_QCOM|PID_NS|IPC_NS|USER_NS|CGROUP_NS|KPROBES|DEVTMPFS|NAMESPACES)[= ]" .config
echo "=== module count ==="
grep -c "=m" .config

echo "=== build Image ==="
# purge .cmd caches corrupted by the earlier concurrent-build accident
find . -name '.*.cmd' -type f -exec grep -qP '\x00' {} \; -delete
MLOG=/mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-make.log
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- KCFLAGS=-Wno-error Image > $MLOG 2>&1
MAKE_RC=$?
echo "make exit=$MAKE_RC"
grep -nE "error:|Error [0-9]" $MLOG | head -8
tail -4 $MLOG

if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUTDIR/Image-p1
  ls -la $OUTDIR
  echo BUILD_OK
else
  echo BUILD_FAILED
fi
echo SCRIPT_END
