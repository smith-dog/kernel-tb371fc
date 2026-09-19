#!/bin/bash
# p48 — v27-q706: 根因修复构建 = p46 配方, 唯一改动 CLANG_TRIPLE=android→gnu
# 根因(2026-09-17 定案): Snapdragon LLVM 10.0.7 (AOSP fork) 对 android triple
#   默认生成 TLS 栈金丝雀 (mrs tpidr_el0; ldr [x22,#40]); 引导链从未初始化
#   TPIDR_EL0 ⇒ kaslr_early_init/start_kernel prologue 读未映射地址 ⇒ 同步异常
#   ⇒ 零标记无声死亡 (v19~v26 全族)。bjsl08 = adrp __stack_chk_guard (global)
#   ⇒ 联想实际用 gnu 类 triple 构建。gnu triple 实测生成 global 金丝雀 ✓
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p48-v27-gnutriple.log 2>&1
echo "=== P48 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. 基座 = bjsl08 config + olddefconfig (gnu triple) ==="
cp $BASE/out/config-tw-bjsl08.txt .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as olddefconfig 2>&1 | tail -2

echo "=== 2. fragment (与 p37/p46 同款) ==="
./scripts/config \
  -e NAMESPACES -e PID_NS -e SYSVIPC -e IPC_NS -e USER_NS -e NET_NS -e CGROUP_NS \
  -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e KPROBES \
  -e BRIDGE_NETFILTER -e CGROUP_PIDS -e CGROUP_DEVICE \
  -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG \
  -e SENSORS_HALL \
  -e TOUCHSCREEN_GOODIX_BRL -e GOODIX_BRL_I2C -e GOODIX_BRL_TOOLS \
  -e GOODIX_BRL_GESTURE -e GOODIX_BRL_INSPECT \
  -d MODVERSIONS -d QCOM_MINIDUMP
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as olddefconfig 2>&1 | tail -2
echo "--- fragment sanity ---"
grep -E "^CONFIG_PID_NS=|^CONFIG_IPC_NS=|^CONFIG_SYSVIPC=|^CONFIG_PSTORE_RAM=|^# CONFIG_MODVERSIONS|^# CONFIG_QCOM_MINIDUMP" .config

echo "=== 3. clean + 全量重建 (gnu triple) ==="
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean > /dev/null 2>&1
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p48-make.log 2>&1
MK=$?
echo "make exit=$MK (took $SECONDS s)"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|No rule to make target|Error [0-9]" $BASE/logs/p48-make.log | head -12
  echo BUILD_FAILED; exit 1
fi
cp arch/arm64/boot/Image $BASE/out/Image-v27q706
cp System.map $BASE/out/System.map-v27q706
ls -la $BASE/out/Image-v27q706
echo "--- 符号数 (bjsl08=121379 对照, v26=119280) ---"
wc -l < System.map

echo "=== 4. 金丝雀验证: start_kernel/kaslr_early_init prologue 必须是 adrp __stack_chk_guard ==="
python3 - <<'EOF'
import subprocess, struct
BASE = '/mnt/d/work/code-work/project/tb371fc-kernel'
img = open(f'{BASE}/out/Image-v27q706', 'rb').read()
smap = open('/home/smith/android_kernel_lenovo_paladin/System.map').read().split()
addrs = {}
vals = smap
for i in range(0, len(vals) - 2, 3):
    try: addrs[vals[i+2]] = int(vals[i], 16)
    except ValueError: pass
TEXT = 0xffffff8008080000
for fn in ('start_kernel', 'kaslr_early_init'):
    a = addrs.get(fn)
    if not a: print(f'{fn}: NOT FOUND'); continue
    off = a - TEXT
    words = [struct.unpack_from('<I', img, off + i*4)[0] for i in range(24)]
    has_tpidr = any((w & 0xFFFFFFE0) == 0xD53BD040 or (w & 0xFFFFFFE0) == 0xD53BD056 or w == 0xD53BD056 for w in words)
    # mrs xN, tpidr_el0 = 0xd53bd040 | (N<<5)? 通用判定: 0xd53bd0.. 段
    tpidr = any((w >> 5) == 0x6A9E82 for w in words)
    print(f'{fn}: tpidr_read={tpidr}  (must be False); first words: {[hex(w) for w in words[:6]]}')
EOF

echo "=== 5. repack boot-v27 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v27q706 $BASE/out/boot-v27-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
md5sum $BASE/out/boot-v27-q706.img
echo "=== P48 DONE $(date) ==="
