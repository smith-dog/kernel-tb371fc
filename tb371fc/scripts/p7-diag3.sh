#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p7-diag3.log 2>&1
K2=/home/smith/kernel-200
KV=/home/smith/kernel-van
echo "=== drm_notifier_mi.h location ==="
find $K2/include $K2/techpack -name "drm_notifier_mi.h" 2>/dev/null | head -3
echo "=== who includes it in vanilla copy ==="
grep -rln "drm_notifier_mi" $KV/techpack/display/msm/ 2>/dev/null | head -6
echo "=== backlight.c diff size ==="
wc -l $K2/drivers/video/backlight/backlight.c $KV/drivers/video/backlight/backlight.c
diff $KV/drivers/video/backlight/backlight.c $K2/drivers/video/backlight/backlight.c | grep -cE "^[<>]"
echo "=== get_by_type_a usage sites ==="
grep -rn "get_by_type_a\|get_by_type_b" $KV/techpack/display/msm/dsi/*.c | head -4
echo "=== other mi headers used by display msm ==="
grep -rhoE "#include [\"<][^\">]*mi[^\">]*[\">]" $KV/techpack/display/msm/ 2>/dev/null | sort -u | head -8
echo DIAG3_DONE
