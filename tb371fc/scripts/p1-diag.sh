#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-diag.log 2>&1
KDIR=/home/smith/android_kernel_xiaomi_sm8250
cd $KDIR
export PATH=/usr/lib/llvm-17/bin:$PATH
echo ===PLL-TAIL===
tail -3 techpack/display/pll/Makefile
echo ===UFS-MAKEFILE===
grep -nE "Werror|CFLAGS|ccflags" drivers/scsi/ufs/Makefile
echo ===V1-PLL===
make V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- KCFLAGS=-Wno-error techpack/display/pll/dsi_pll_10nm.o 2>&1 | grep -E "clang|pll_trace|error" | head -6
echo DIAG_DONE
