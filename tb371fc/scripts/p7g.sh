#!/bin/bash
# v7g: restore clone_cooling_device (headers now present) + port mi_drm_notifier
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7g.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
echo "=== where is mi_drm_notifier_call_chain defined in 23.2 ==="
grep -rln "mi_drm_notifier_call_chain" $K2/drivers $K2/include $K2/techpack 2>/dev/null | head -5
echo "=== restore clone_cooling_device obj entry ==="
M=$KV/techpack/display/msm/Makefile
grep -q "clone_cooling_device" $M || sed -i 's|\(sde/.*\.o \)\?$|&|' $M
# re-add the line after sde_backlight.o entry (find original position pattern)
grep -n "sde_backlight\|sde_ad4" $M | head -4
echo PATCH_DONE
