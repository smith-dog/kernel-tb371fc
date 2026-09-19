#!/bin/bash
# p30-paladin.sh — v19-q706: Lenovo paladin(Q706F/Q706Z) 源码树首build
# 目标: 4.19.157 Lenovo kona 源码 + namespace/Docker fragment + pstore 取证
# 产物: out/Image-v19q706, out/boot-v19-q706.img
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p30-paladin.log 2>&1
echo "=== P30 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

echo "=== 1. extract tarball (symlink-safe, WSL native fs) ==="
if [ ! -d $KV ]; then
  mkdir -p $KV
  tar -xzf $BASE/reference/paladin-11.tar.gz -C $KV --strip-components=1 || { echo EXTRACT_FAIL; exit 1; }
fi
ls -la $KV/arch/arm64/configs/vendor/paladin_row_lte-perf_defconfig || echo "WARN: paladin symlink missing"
cd $KV || exit 1

echo "=== 2. defconfig: vendor/kona-perf_defconfig ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- vendor/kona-perf_defconfig || { echo DEFCONFIG_FAIL; exit 1; }

echo "=== 3. fragment: namespace + Docker + pstore; off: MODVERSIONS/MINIDUMP ==="
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -d MODVERSIONS -d QCOM_MINIDUMP
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig

echo "=== 4. verify key configs ==="
grep -E "^CONFIG_(NAMESPACES|PID_NS|IPC_NS|USER_NS|NET_NS|CGROUP_NS|SYSVIPC|DEVTMPFS|KPROBES|BRIDGE_NETFILTER|CGROUP_PIDS|CGROUP_DEVICE|PSTORE_RAM|PSTORE_CONSOLE|MODVERSIONS|QCOM_MINIDUMP|IKCONFIG|IKCONFIG_PROC|TOUCHSCREEN_FTS|OVERLAY_FS|VETH|BRIDGE|TUN)\b" .config | sort

echo "=== 5. build Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p30-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "error:|undefined reference|No rule to make target" $BASE/logs/p30-make.log | head -8
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v19q706
ls -la $BASE/out/Image-v19q706

echo "=== 6. repack boot (APatch base + stock ZUI ramdisk + no-map DTB tail) ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v19q706 $BASE/out/boot-v19-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v19-q706.img
md5sum $BASE/out/boot-v19-q706.img
echo "=== P30 DONE $(date) ==="
