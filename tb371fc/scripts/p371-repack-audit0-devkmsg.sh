#!/bin/bash
# p371 — v1.8 final repack: cmdline audit=0 + printk.devkmsg=off
# Root-cause fix for aplog kernellog flood (645MB/day -> ~normal):
# 1. ZUI vendor HAL (vendor.stc.hard) spams avc denials -> audit=0 silences
#    at source (SELinux stays Enforcing, only the logger is off).
# 2. healthd/QCOM-BATT/binder heartbeats refill via unfiltered /dev/kmsg
#    reads -> printk.devkmsg=off.
# Measured on #200: 220k lines/min -> 58 lines/min. No kernel rebuild:
# same Image (#200), cmdline-only repack.
set -e
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
BASE=/mnt/d/work/code-work/project/devices/tb371fc
LOG=$BASE/logs/p371-repack-audit0-devkmsg.log
exec > $LOG 2>&1
echo "=== P371 V1.8 FINAL REPACK START $(date) ==="
cd $BASE
python3 $KV/tb371fc/tools/repack_boot.py \
    out/boot-devclean1-kspatched-flash.img \
    out/Image-v18 \
    out/boot-v1.8-kspatched-flash.img \
    "" \
    "" \
    "audit=0 printk.devkmsg=off"
echo "=== md5 ==="
md5sum out/boot-v1.8-kspatched-flash.img
strings out/Image-v18 | grep -m1 "Linux version"
echo "=== P371 DONE $(date) ==="
