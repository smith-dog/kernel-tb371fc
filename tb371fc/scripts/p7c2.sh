#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7c2.log 2>&1
echo START
KV=/home/smith/kernel-van
cp $KV/include/drm/drm_notifier_mi.h $KV/include/drm/drm_bridge.h $KV/include/drm/drm_mipi_dsi.h $KV/include/linux/backlight.h $KV/drivers/video/backlight/backlight.c /tmp/ 2>/dev/null
ls -la /tmp/*.h /tmp/backlight.c 2>/dev/null | head -6
echo CHECK_DONE
