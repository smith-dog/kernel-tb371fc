#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p3-rtic.log 2>&1
cd /home/smith/kernel-van || exit 1
grep -rn "bss.rtic" security/selinux/ | head -4
grep -n "rtic\|RTIC" security/selinux/Makefile | head -4
grep -iE "RTIC|RTCT" arch/arm64/configs/vendor/kona-perf_defconfig | head -4
grep -rn "config QCOM_RTIC\|config RTIC" drivers/ Kconfig 2>/dev/null | head -3
grep -rn "rtic" security/selinux/hooks.c | head -3
echo RTIC_DONE
