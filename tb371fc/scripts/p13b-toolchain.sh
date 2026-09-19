#!/bin/bash
# Extract none-elf toolchain in WSL, rebuild KPM with it
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p13b.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
rm -rf /home/smith/arm-gcc
mkdir -p /home/smith/arm-gcc
tar -Jxf $BASE/arm-gcc.tar.xz -C /home/smith/arm-gcc
TC=$(echo /home/smith/arm-gcc/*/bin/aarch64-none-elf-)
echo "TC=$TC"
ls ${TC}gcc >/dev/null 2>&1 && echo GCC_OK || { echo GCC_MISSING; exit 1; }

cd $BASE/kpms/tb371fc-dump || exit 1
export TARGET_COMPILE=$TC
make clean >> /dev/null 2>&1
make 2>&1 | tail -4
ls -la tb371fc-dump.kpm 2>/dev/null && echo KPM_OK

# deploy to device (protected boot running with adb root)
adb wait-for-device 2>/dev/null
adb push tb371fc-dump.kpm /data/local/tmp/tb371fc-dump-noneelf.kpm 2>&1 | tail -1
adb shell "su -c 'cp /data/local/tmp/tb371fc-dump-noneelf.kpm /data/adb/ap/kpm/tb371fc-dump/tb371fc-dump.kpm && ls -la /data/adb/ap/kpm/tb371fc-dump/'" 2>&1 | tr -d '\r' | head -3
echo CHAIN_DONE
