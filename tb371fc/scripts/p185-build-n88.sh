#!/bin/bash
# p185-build-n88.sh — v27n88 = n87 - CONFIG_SYSVIPC (back to VINTF-clean).
# Steps: p184 config edit -> olddefconfig (dependency close: drops
# SYSVIPC_SYSCTL/SYSVIPC_COMPAT, keeps IPC_NS via POSIX_MQUEUE) -> full
# Image build -> repack pure + patched-ramdisk images.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p185-build.log 2>&1
echo "=== P185 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
export PATH=$SNAP/bin:$PATH
cd $KV || { echo NO_TREE; exit 1; }

python3 $BASE/p184-sysvipc-off.py $KV/.config || { echo P184_FAILED; exit 1; }
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang olddefconfig > /tmp/p185-olddef.log 2>&1
echo "olddefconfig exit=$?"
echo "--- config check ---"
grep -E '^CONFIG_SYSVIPC|CONFIG_POSIX_MQUEUE=|^# CONFIG_SYSVIPC|CONFIG_IPC_NS=' .config
if grep -q '^CONFIG_SYSVIPC=y' .config; then echo SYSVIPC_STILL_ON; exit 1; fi
grep -q '^CONFIG_IPC_NS=y' .config || { echo IPC_NS_LOST; exit 1; }

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > /tmp/p185-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; grep -nE "error:" /tmp/p185-make.log | head -10; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n88

cd $BASE
[ -s out/ramdisk-114142.img ] || echo "WARN_NO_RAMDISK (patched image skipped)"
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n88 out/boot-v27n88-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
if [ -s out/ramdisk-114142.img ]; then
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n88 out/boot-v27n88-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" 2>&1 | tail -2
fi
echo "=== md5 ==="
md5sum out/Image-v27n88 out/boot-v27n88-*.img
echo "=== P185 BUILD DONE $(date) ==="
