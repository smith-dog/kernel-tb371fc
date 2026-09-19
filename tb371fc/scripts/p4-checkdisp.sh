#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p4-checkdisp.log 2>&1
cd /home/smith/kernel-van || exit 1
echo "=== driver source for dsi-ctrl compatible exists? ==="
grep -rln "dsi-ctrl-hw-v2.4" drivers/ techpack/ 2>/dev/null | head -5
grep -rln "dsi-phy-v4.1" drivers/ techpack/ 2>/dev/null | head -5
echo "=== techpack contents ==="
ls techpack/
ls techpack/display 2>/dev/null | head -10
echo "=== msm dsi dir ==="
ls techpack/display/msm/dsi/ 2>/dev/null | head -8
ls drivers/gpu/drm/msm/ 2>/dev/null | head -8
echo "=== config: display options in our .config ==="
grep -E "TECHPACK|DSI|MDSS|SDE" .config | grep -v "^#" | head -12
echo CHECKDISP_DONE
