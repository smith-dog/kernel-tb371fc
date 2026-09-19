#!/bin/bash
# Build the tb371fc-dump KPM
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p12-dumpkpm.log 2>&1
echo START
cd /mnt/d/work/code-work/project/tb371fc-kernel/kpms/tb371fc-dump || exit 1
export TARGET_COMPILE=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
make clean >> /dev/null 2>&1
make 2>&1 | tail -6
ls -la tb371fc-dump.kpm 2>/dev/null && echo KPM_DUMP_OK || echo KPM_DUMP_FAILED
echo DONE
