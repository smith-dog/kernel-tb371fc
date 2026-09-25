#!/bin/bash
# p212-build-n95.sh — TASK-034 P3: CLO-source qcacld-3.0 trio into drivers/staging
# (built-in WLAN, CONFIG_QCA_CLD_WLAN=y), replacing the Lenovo prebuilt wlan.ko.
# Kernel-side n94 changes (audio built-in p210, 144 port p200/p206/p208) all retained.
exec > /mnt/d/work/code-work/project/devices/tb371fc/logs/p212-build.log 2>&1
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
T34=/home/smith/t34
BASE=/mnt/d/work/code-work/project/devices/tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7

echo "=== P212 BUILD START $(date) ==="
set -e

echo "--- copy trio into staging ---"
rm -rf $KV/drivers/staging/qcacld-3.0 $KV/drivers/staging/qca-wifi-host-cmn $KV/drivers/staging/fw-api
cp -a $T34/qcacld-3.0        $KV/drivers/staging/qcacld-3.0
cp -a $T34/qca-wifi-host-cmn $KV/drivers/staging/qca-wifi-host-cmn
cp -a $T34/fw-api-full       $KV/drivers/staging/fw-api
rm -rf $KV/drivers/staging/qcacld-3.0/.git $KV/drivers/staging/qca-wifi-host-cmn/.git $KV/drivers/staging/fw-api/.git
ls $KV/drivers/staging/

echo "--- wire staging Kconfig/Makefile ---"
grep -q 'qcacld-3.0/Kconfig' $KV/drivers/staging/Kconfig || echo 'source "drivers/staging/qcacld-3.0/Kconfig"' >> $KV/drivers/staging/Kconfig
grep -q 'CONFIG_QCA_CLD_WLAN' $KV/drivers/staging/Makefile || echo 'obj-$(CONFIG_QCA_CLD_WLAN) += qcacld-3.0/' >> $KV/drivers/staging/Makefile
tail -2 $KV/drivers/staging/Kconfig; tail -2 $KV/drivers/staging/Makefile

echo "--- enable CONFIG_QCA_CLD_WLAN ---"
export PATH=$SNAP/bin:$PATH
cd $KV
grep -q '^CONFIG_QCA_CLD_WLAN=' .config || echo 'CONFIG_QCA_CLD_WLAN=y' >> .config
grep -q '^CONFIG_CNSS_QCA6390=' .config || echo 'CONFIG_CNSS_QCA6390=y' >> .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E 'CONFIG_QCA_CLD_WLAN|CONFIG_CNSS_QCA6390' .config

echo "--- build Image ---"
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS="-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time" \
     DYNAMIC_SINGLE_CHIP=qca6390 \
     Image
MK=$?
echo "make Image exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; grep -nE "error:" "$0.log" 2>/dev/null | head -10; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n95

cd $BASE
python3 tools/repack_boot.py firmware/apatch_patched_11266_0.13.5_clte.img \
    out/Image-v27n95 out/boot-v27n95-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
python3 tools/repack_boot.py out/kernelsu_patched_20260924_084550.img \
    out/Image-v27n95 out/boot-v27n95-kspatched-flash.img out/dtbs/tail-new.bin 2>&1 | tail -2
echo "=== banner ==="
strings out/Image-v27n95 | grep -m1 "Linux version"
echo "=== md5 ==="
md5sum out/Image-v27n95 out/boot-v27n95-pure-q706.img out/boot-v27n95-kspatched-flash.img
echo "=== P212 BUILD DONE $(date) ==="
