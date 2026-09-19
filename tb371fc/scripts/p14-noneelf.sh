#!/bin/bash
# Rebuild tb371fc-ramoops KPM with the none-elf toolchain (no libc symbols)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p14-noneelf.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
ls ${TC}gcc >/dev/null 2>&1 && echo TC_OK || { echo TC_MISSING; exit 1; }
${TC}gcc --version | head -1

cd $BASE/kpms/tb371fc-ramoops || exit 1
make clean >> /dev/null 2>&1
TARGET_COMPILE=$TC make 2>&1 | tail -3
ls -la tb371fc-ramoops.kpm 2>/dev/null && echo KPM_NONEELF_OK

# check for libc symbol references (must be ZERO)
aarch64-linux-gnu-nm tb371fc-ramoops.kpm 2>/dev/null | grep -iE "strlcpy|strncat|memcpy|memset" | head -3
echo "libc-refs-done"

adb push tb371fc-ramoops.kpm /data/local/tmp/tb371fc-ramoops-noneelf.kpm 2>&1 | tail -1
adb shell "su -c 'cp /data/local/tmp/tb371fc-ramoops-noneelf.kpm /data/adb/ap/kpm/tb371fc-ramoops/tb371fc-ramoops.kpm; sync'" 2>&1 | tr -d '\r' | head -1
echo NONEELF_KPM_DEPLOYED
