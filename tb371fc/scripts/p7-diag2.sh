#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7-diag2.log 2>&1
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
echo "=== vanilla uapi sde_drm.h? ==="
ls -la $KV/include/uapi/drm/sde_drm.h 2>/dev/null || echo "vanilla: no sde_drm.h"
ls -la $K2/include/uapi/drm/sde_drm.h 2>/dev/null
echo "=== clone_cooling gating in msm Makefile ==="
grep -n "clone" $K2/techpack/display/msm/Makefile | head -3
echo "=== clone_cooling includes ==="
head -20 $K2/techpack/display/msm/clone_cooling_device.c | grep "#include"
echo "=== 23.2 backlight.h vs vanilla diff (just size) ==="
wc -l $K2/include/linux/backlight.h $KV/include/linux/backlight.h
diff $KV/include/linux/backlight.h $K2/include/linux/backlight.h | head -30
echo DIAG2_DONE
