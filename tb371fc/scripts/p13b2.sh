#!/bin/bash
# Build demo-hello with the none-elf toolchain and deploy-test
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p13b2.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
if [ ! -d /home/smith/arm-gcc ]; then
  mkdir -p /home/smith/arm-gcc
  tar -Jxf $BASE/arm-gcc.tar.xz -C /home/smith/arm-gcc
fi
TC=$(ls -d /home/smith/arm-gcc/*/bin/aarch64-none-elf- 2>/dev/null | head -1)
echo "TC=$TC"
[ -n "$TC" ] || { echo NO_TC; exit 1; }
${TC}gcc --version | head -1

cd $BASE/reference/KernelPatch-main/kpms/demo-hello || exit 1
make clean >> /dev/null 2>&1
TARGET_COMPILE=$TC make 2>&1 | tail -3
ls -la hello.kpm 2>/dev/null && echo DEMO_NONEELF_OK

# deploy to device
adb push hello.kpm /data/local/tmp/demo-hello-noneelf.kpm 2>&1 | tail -1
adb shell "su -c 'cp /data/local/tmp/demo-hello-noneelf.kpm /data/adb/ap/kpm/demo-hello/demo-hello.kpm; sync'" 2>&1 | tr -d '\r' | head -1
echo NONEELF_DEMO_DONE
