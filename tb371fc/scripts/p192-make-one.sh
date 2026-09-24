#!/bin/bash
# p192-make-one.sh — V=1 dry-run/single-object build diagnostics for techpack audio
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
cd "$KV" || exit 1
OUT=/mnt/d/work/code-work/tmp_v1.txt
echo "START" > "$OUT"
make -n V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     techpack/audio/soc/pinctrl-wcd.o >> "$OUT" 2>&1
echo "DRYRUN_EXIT=$?" >> "$OUT"
make V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     techpack/audio/soc/pinctrl-wcd.o >> "$OUT" 2>&1
echo "REAL_EXIT=$?" >> "$OUT"
