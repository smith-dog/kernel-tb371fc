#!/bin/bash
# p193-clang-v.txt generator: rerun exact pinctrl-wcd compile with -v to inspect include search
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
cd "$KV" || exit 1
OUT=/mnt/d/work/code-work/tmp_v1.txt
{
ls -la techpack/audio/soc/core.h
head -3 techpack/audio/soc/core.h
file techpack/audio/soc/core.h
echo ---L18---
sed -n '14,22p' techpack/audio/soc/pinctrl-wcd.c
} > "$OUT" 2>&1

# minimal repro: same flags (trimmed to essentials), clang -v, only -c -o /dev/null
clang -nostdinc -isystem /usr/lib/llvm-21/lib/clang/21/include -I./arch/arm64/include -I./arch/arm64/include/generated -I./include -I./arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -std=gnu89 --target=aarch64-linux-gnu -DCONFIG_ARCH_KONA=y -I./techpack/audio/include -I./techpack/audio/include/uapi -v -c -o /dev/null techpack/audio/soc/pinctrl-wcd.c >> "$OUT" 2>&1
echo "CLANG_EXIT=$?" >> "$OUT"
