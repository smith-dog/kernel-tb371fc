#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7-diag.log 2>&1
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
echo "=== FOD definition in 23.2 tree ==="
grep -rn "FOD_PRESSED_LAYER_ZORDER" $K2/drivers/techpack $K2/techpack $K2/include 2>/dev/null | grep -iE "define" | head -3
grep -rn "define FOD_PRESSED_LAYER_ZORDER" $K2 2>/dev/null | head -3
echo "=== clone_cooling_device gating ==="
grep -n "clone_cooling_device" $K2/techpack/display/msm/sde/Makefile | head -3
grep -rn "thermal_brightness_clone_limit" $K2/include | head -3
echo "=== what gates them in config ==="
grep -E "CLONE|FOD" /home/smith/kernel-200/.config | head -6
grep -E "CLONE|FOD" $KV/.config | head -6
echo DIAG_DONE
