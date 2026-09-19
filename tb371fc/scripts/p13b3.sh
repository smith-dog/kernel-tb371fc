#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p13b3.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-

cd $BASE/reference/KernelPatch-main/kpms/demo-hello || exit 1
make clean >> /dev/null 2>&1
TARGET_COMPILE=$TC CFLAGS="-fno-stack-protector" make 2>&1 | tail -2
ls -la hello.kpm 2>/dev/null && echo DEMO_OK
aarch64-linux-gnu-objdump -h hello.kpm 2>/dev/null | grep -E "kpm" | head -6

adb push hello.kpm /data/local/tmp/demo-nsp.kpm 2>&1 | tail -1
adb shell "su -c 'cp /data/local/tmp/demo-nsp.kpm /data/adb/ap/kpm/demo-hello/demo-hello.kpm; sync'" 2>&1 | tr -d '\r' | head -1
echo NONEELF_NSP_DEPLOYED
