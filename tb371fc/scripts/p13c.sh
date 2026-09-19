#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p13c.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
cd $BASE/reference/KernelPatch-main/kpms/demo-hello || exit 1
INC="-I. -Iinclude -I../../patch/include -I../../linux/include -I../../linux/arch/arm64/include -I../../linux/tools/arch/arm64/include"

echo "=== test1: no flags ==="
${TC}gcc $INC -c -O2 -o /tmp/t1.o hello.c 2>/dev/null
aarch64-linux-gnu-nm /tmp/t1.o 2>/dev/null | grep -c stack_chk

echo "=== test2: -fno-stack-protector ==="
${TC}gcc -fno-stack-protector $INC -c -O2 -o /tmp/t2.o hello.c 2>/dev/null
aarch64-linux-gnu-nm /tmp/t2.o 2>/dev/null | grep -c stack_chk

echo "=== test3: -fno-stack-protector with kernel-style includes ==="
${TC}gcc -fno-stack-protector -nostdinc $INC -c -O2 -o /tmp/t3.o hello.c 2>/dev/null
aarch64-linux-gnu-nm /tmp/t3.o 2>/dev/null | grep -c stack_chk

echo "=== default ssp check ==="
${TC}gcc -Q --help=common 2>/dev/null | grep -iE "stack-protector" | head -3
echo TEST_DONE
