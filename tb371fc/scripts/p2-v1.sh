#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-v1.log 2>&1
cd /home/smith/kernel-200 || exit 1
export PATH=/usr/lib/llvm-17/bin:$PATH
make V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- techpack/display/pll/dsi_pll_10nm.o 2>&1 | grep -oE "\-I[^ ]*pll[^ ]*|fatal error[^ ]*" | head -3
echo === HID ===
make V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- drivers/hid/hid-trace.o 2>&1 | grep -E "clang.*hid-trace|fatal error" | tail -2 | cut -c1-300
echo V1_DONE
