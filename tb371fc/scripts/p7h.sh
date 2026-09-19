#!/bin/bash
# v7h: restore clone line, copy drm_notifier_mi.c, wire into build, rebuild
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7h.log 2>&1
echo START
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
# restore msm/Makefile from 23.2 (has clone_cooling_device entry)
cp $K2/techpack/display/msm/Makefile $KV/techpack/display/msm/Makefile && echo mk_restored
# copy mi notifier source
cp $K2/drivers/gpu/drm/drm_notifier_mi.c $KV/drivers/gpu/drm/drm_notifier_mi.c && echo notifier_copied
echo "=== 23.2 drm Makefile entry for notifier ==="
grep -n "drm_notifier_mi" $K2/drivers/gpu/drm/Makefile
grep -n "drm_notifier_mi" $KV/drivers/gpu/drm/Makefile || {
  # add to vanilla Makefile same way (need the config gate from 23.2 Kconfig)
  grep -rn "MI_DRM\|DRM_NOTIFIER_MI" $K2/drivers/gpu/drm/Kconfig | head -3
}
echo "=== vanilla drm makefile tail ==="
tail -5 $KV/drivers/gpu/drm/Makefile
echo DIAG_DONE
