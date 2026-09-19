#!/bin/bash
# p37 — v21-q706: GCC12.2 工具链全量重建（判定 clang-17 误编译假设）
# 同一棵树(paladin+display+全部修复), 只换编译器。clang→gcc 必须 clean。
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p37-gcc.log 2>&1
echo "=== P37 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
TC=/usr/local/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin
cd $KV || exit 1

echo "=== 1. extract arm-gcc 12.2 ==="
if [ ! -d "$TC" ]; then
  tar -xJf $BASE/arm-gcc.tar.xz -C /usr/local/ || { echo EXTRACT_FAIL; exit 1; }
fi
export PATH=$TC:$PATH
aarch64-none-elf-gcc --version | head -1

echo "=== 2. clean (clang objects incompatible with gcc) ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- clean > /dev/null 2>&1

echo "=== 3. olddefconfig (same .config base) ==="
# 注意: 树的 Makefile:412 默认 CC=python2 gcc-wrapper.py(警告过滤), 显式绕过
make ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- CC=aarch64-none-elf-gcc vendor/kona-perf_defconfig
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -d MODVERSIONS -d QCOM_MINIDUMP
make ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- CC=aarch64-none-elf-gcc olddefconfig

echo "=== 4. build Image with gcc 12.2 ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- CC=aarch64-none-elf-gcc KCFLAGS=-Wno-error Image > $BASE/logs/p37-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target|Error " $BASE/logs/p37-make.log | grep -v "Werror\|-Werror" | head -10
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v21q706
ls -la $BASE/out/Image-v21q706

echo "=== 5. repack boot ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v21q706 $BASE/out/boot-v21-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v21-q706.img
md5sum $BASE/out/boot-v21-q706.img
echo "=== P37 DONE $(date) ==="
