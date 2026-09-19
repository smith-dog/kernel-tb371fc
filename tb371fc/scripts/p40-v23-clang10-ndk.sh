#!/bin/bash
# p40 — v23-q706: clang 10.0.7 (NDK r21d) 同代差重建
# 假设: v19~v22 全灭的剩余主假设 = 编译器代差 (clang-17 / GCC-12.2 对 4.19 早期
#       代码的误译)。bjsl08 构建串 = "clang 10.0.7 NDK"；NDK r21d 自带 clang 即
#       10.0.7。本脚本: 同一棵树、同一 defconfig+fragment、同一打包配方，只换 CC。
# 运行方式: wsl -u root -e bash <本文件>   (构建树 root 所有, root 免密)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p40-v23-clang10.log 2>&1
echo "=== P40 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
NDKZIP=/home/smith/android-ndk-r21d-linux-x86_64.zip
NDK=/home/smith/android-ndk-r21d
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin
cd $KV || { echo NO_TREE; exit 1; }

echo "=== 1. download NDK r21d (clang 10.0.7, ~1.1GB, 断点续传; 腾讯镜像优先) ==="
if [ ! -x $TC/clang ]; then
  if [ ! -f $NDKZIP ] || [ $(stat -c %s $NDKZIP) -lt 1000000000 ]; then
    ok=0
    for u in \
      "https://mirrors.cloud.tencent.com/AndroidSDK/android-ndk-r21d-linux-x86_64.zip" \
      "https://dl.google.com/android/repository/android-ndk-r21d-linux-x86_64.zip"; do
      echo "-- try $u"
      curl -L -C - --retry 3 --retry-delay 5 -m 3600 -o $NDKZIP "$u" \
        && { ok=1; break; } || echo "-- failed, next source"
    done
    [ $ok -eq 1 ] || { echo DL_FAIL; exit 1; }
  fi
  ls -la $NDKZIP
  echo "=== 1b. unzip ==="
  if ! which unzip >/dev/null 2>&1; then
    apt-get install -y unzip >/dev/null 2>&1 || { echo APT_UNZIP_FAIL; }
  fi
  if which unzip >/dev/null 2>&1; then
    unzip -q -o $NDKZIP -d /home/smith/ || { echo UNZIP_FAIL; exit 1; }
  else
    # 兜底: python zipfile 不保 exec 位, 解完统一补
    python3 -m zipfile -e $NDKZIP /home/smith/ || { echo UNZIP_FAIL_PY; exit 1; }
    find $NDK -type f -exec chmod +x {} \; 2>/dev/null
  fi
fi
[ -x $TC/clang ] || { echo NO_CLANG_BIN; exit 1; }
echo "=== 1c. clang version (必须含 10.0.7) ==="
$TC/clang --version || { echo CLANG_RUN_FAIL; exit 1; }
$TC/clang --version | grep -q "10\.0\.7" || { echo WRONG_VERSION; exit 1; }

echo "=== 2. clean (gcc 对象与 clang 不兼容, 同 p37) ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean > /dev/null 2>&1
echo "clean done"

echo "=== 3. defconfig + fragment (与 p37 完全一致, 但用 clang 做 kconfig 探测) ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     vendor/kona-perf_defconfig || { echo DEFCONFIG_FAIL; exit 1; }
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -d MODVERSIONS -d QCOM_MINIDUMP
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as olddefconfig
echo "--- fragment sanity ---"
grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_PSTORE_RAM=|^# CONFIG_MODVERSIONS|^# CONFIG_QCOM_MINIDUMP" .config

echo "=== 4. build Image with clang 10.0.7 ==="
export PATH=$TC:$PATH
which clang
clang --version | head -1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p40-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  echo "--- first errors ---"
  grep -nE "fatal error:|error:|undefined reference|relocation truncated|No rule to make target|Error [0-9]" $BASE/logs/p40-make.log | head -12
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
ls -la arch/arm64/boot/Image

echo "=== 5. artifact checks ==="
cp arch/arm64/boot/Image $BASE/out/Image-v23q706
ls -la $BASE/out/Image-v23q706
echo "--- symbol count (v21=139131, v19=123204 对照) ---"
wc -l < System.map
echo "--- 黑匣子在 Image 内 (应为 1+) ---"
grep -c TB371FC_BLACKBOX kernel/printk/printk.c
grep -c tb_bb_copy init/main.c
echo "--- 实际编译器版本回读 (构建日志首部) ---"
grep -m1 "clang version" $BASE/logs/p40-make.log

echo "=== 6. 内嵌 config 自验证 (p29 IKCONFIG 直提) ==="
python3 $BASE/scripts/p29-extract-ikconfig.py $BASE/out/Image-v23q706 \
    > $BASE/out/config-v23-extracted.txt 2>&1 \
  && grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_USER_NS=|^CONFIG_BRIDGE_NETFILTER=|^CONFIG_CGROUP_PIDS=|^CONFIG_PSTORE_RAM=" $BASE/out/config-v23-extracted.txt \
  || echo IKCONFIG_EXTRACT_FAIL(非致命,记录即可)

echo "=== 7. repack boot (APatch 基板 + ZUI ramdisk + tail-new DTB, 与 v19~v22 同配方) ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v23q706 $BASE/out/boot-v23-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
ls -la $BASE/out/boot-v23-q706.img
md5sum $BASE/out/boot-v23-q706.img
echo "=== P40 DONE $(date) ==="
