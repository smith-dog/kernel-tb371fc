#!/bin/bash
# p42 — v23-q706: Snapdragon LLVM ARM Compiler 10.0.7 (for Android NDK) 全量重建
# 与 v19~v22 的差异 = 工具链一代: clang-17/GCC-12.2 → 高通 Snapdragon LLVM 10.0.7
#   + 配套 aarch64-linux-android binutils 2.27 (bjsl08 横幅内的同一套)。
# 树/配置/打包配方与 p37(v21)/p39(v22) 完全一致。
# 前置: p41 已部署 /home/smith/snapdragon-llvm-10.0.7 且横幅校验通过。
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p42-v23-snapllvm.log 2>&1
echo "=== P42 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }

echo "=== 0. toolchain gate ==="
$SNAP/bin/clang --version | head -2 || { echo NO_SNAP; exit 1; }
$SNAP/bin/clang --version | grep -q "clang version 10.0.7 for Android NDK" || { echo WRONG_VERSION; exit 1; }
export PATH=$SNAP/bin:$PATH
which clang
# 说明: Snapdragon 包不带目标端 GNU binutils(bjsl08 横幅里的 binutils-2.27 是
# 高通另一组分发包, 未公开)。as/ld 用系统 aarch64-linux-gnu- (v19~v22 同款,
# 跨构建常量, 不影响编译器 A/B); clang 目标三元组 = aarch64-linux-android (同联想)。

echo "=== 1. clean (跨工具链必须 clean, 同 p37) ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean > /dev/null 2>&1
echo "clean done"

echo "=== 2. defconfig + fragment (与 p37 完全一致) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as \
     vendor/kona-perf_defconfig || { echo DEFCONFIG_FAIL; exit 1; }
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -d MODVERSIONS -d QCOM_MINIDUMP
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as olddefconfig
echo "--- fragment sanity ---"
grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_PSTORE_RAM=|^# CONFIG_MODVERSIONS|^# CONFIG_QCOM_MINIDUMP" .config

echo "=== 3. build Image with Snapdragon LLVM 10.0.7 ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p42-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target|Error [0-9]" $BASE/logs/p42-make.log | head -12
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
ls -la arch/arm64/boot/Image

echo "=== 4. 构建产物自验证 ==="
cp arch/arm64/boot/Image $BASE/out/Image-v23q706
ls -la $BASE/out/Image-v23q706
echo "--- 编译器横幅 (bjsl08 原文: clang version 10.0.7 for Android NDK, GNU ld (binutils-2.27-bd24d23f) 2.27.0.20170315; 我们 ld 部分=系统 binutils, 预期不同) ---"
strings -a $BASE/out/Image-v23q706 | grep -m1 "clang version" || { echo NO_BANNER; exit 1; }
echo "--- ld 版本回读 ---"
aarch64-linux-gnu-ld --version | head -1
echo "--- symbol count (v21=139131 / v19=123204 / bjsl08=121379 对照) ---"
wc -l < System.map
echo "--- namespace 内嵌 config 自验证 (p29 IKCONFIG 直提) ---"
python3 $BASE/scripts/p29-extract-ikconfig.py $BASE/out/Image-v23q706 \
    > $BASE/out/config-v23-extracted.txt 2>&1 \
  && grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_USER_NS=|^CONFIG_BRIDGE_NETFILTER=|^CONFIG_CGROUP_PIDS=|^CONFIG_PSTORE_RAM=" $BASE/out/config-v23-extracted.txt \
  || echo IKCONFIG_EXTRACT_FAIL
echo "--- Image 头字段 (对照 bjsl08: image_size 0x2ab4000 / v22 0x2833000) ---"
python3 - <<'PYEOF'
import struct
d = open('/mnt/d/work/code-work/project/tb371fc-kernel/out/Image-v23q706','rb').read(64)
magic, text_off, p2p9, flags = d[56:60], struct.unpack('<I', d[8:12])[0], d[24:32], struct.unpack('<Q', d[32:40])[0]
img_size = struct.unpack('<Q', d[40:48])[0]
print(f'magic={magic} text_offset={text_off:#x} image_size={img_size:#x} flags={flags:#x}')
PYEOF

echo "=== 5. repack boot (APatch 基板 + ZUI ramdisk + tail-new DTB, v19~v22 同配方) ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v23q706 $BASE/out/boot-v23-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
ls -la $BASE/out/boot-v23-q706.img
md5sum $BASE/out/boot-v23-q706.img
echo "=== P42 DONE $(date) ==="
