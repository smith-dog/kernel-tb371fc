#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-diag181.log 2>&1
cd /home/smith/kernel-181 || exit 1
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH
echo "=== HOSTCC handling in Makefile ==="
grep -n "HOSTCC" Makefile | head -8
echo "=== V=1 empty.o ==="
make V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- KCFLAGS=-Wno-error scripts/mod/empty.o 2>&1 | grep -E "clang|as:|error" | head -4
echo DIAG_DONE
