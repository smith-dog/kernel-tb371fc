#!/bin/bash
# p46 — v26-q706: 配置基座对齐 bjsl08 出厂 config (out/config-tw-bjsl08.txt)
# 依据: bjsl08 内核在 v25 管线 18s 正常启动 ⇒ 管线/DTB 无罪; v19~v25 死于内核
#   二进制差异; head.S 逐字节一致 ⇒ 差异在 config 驱动的早期代码 or 树源码。
#   本构建把 52 键 config 差异清零(除 fragment 故意项), 全量重建。
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p46-v26-bjsl08cfg.log 2>&1
echo "=== P46 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. 基座 = bjsl08 config + olddefconfig ==="
cp $BASE/out/config-tw-bjsl08.txt .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as olddefconfig 2>&1 | tail -2

echo "=== 2. fragment (与 p37 同款) ==="
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -d MODVERSIONS -d QCOM_MINIDUMP
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as olddefconfig 2>&1 | tail -2
echo "--- fragment sanity ---"
grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_PSTORE_RAM=|^# CONFIG_MODVERSIONS|^# CONFIG_QCOM_MINIDUMP" .config

echo "=== 3. 残差统计: v26.config vs bjsl08 原始 config ==="
diff <(sort $BASE/out/config-tw-bjsl08.txt) <(sort .config) | grep -cE "^[<>]" || true
diff <(sort $BASE/out/config-tw-bjsl08.txt) <(sort .config) | grep -E "^[<>]" | grep -viE "MODVERSIONS|QCOM_MINIDUMP|PID_NS|IPC_NS|SYSVIPC|USER_NS|CGROUP_NS|NAMESPACES|BRIDGE_NETFILTER|CGROUP_PIDS|CGROUP_DEVICE|PSTORE|DEVTMPFS|KPROBES|NET_NS" | head -30

echo "=== 4. clean + 全量重建 ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean > /dev/null 2>&1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p46-make.log 2>&1
MK=$?
echo "make exit=$MK (took $SECONDS s)"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|No rule to make target|Error [0-9]" $BASE/logs/p46-make.log | head -12
  echo BUILD_FAILED; exit 1
fi
cp arch/arm64/boot/Image $BASE/out/Image-v26q706
ls -la $BASE/out/Image-v26q706
echo "--- 符号数 (bjsl08=121379 对照) ---"
wc -l < System.map
echo "--- 黑匣子完整性 ---"
grep -c "tb_bb_copy\|tb_bb_mark" kernel/printk/printk.c

echo "=== 5. repack boot-v26 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v26q706 $BASE/out/boot-v26-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
md5sum $BASE/out/boot-v26-q706.img
echo "=== P46 DONE $(date) ==="
