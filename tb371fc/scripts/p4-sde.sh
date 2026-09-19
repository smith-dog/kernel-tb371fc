#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p4-sde.log 2>&1
cd /home/smith/kernel-van || exit 1
echo "=== who provides sde ==="
grep -rln "qcom,sde-kms" drivers/ techpack/ 2>/dev/null | head -4
grep -rln "qcom,mdss-dsi-panel" drivers/ techpack/ 2>/dev/null | head -4
echo "=== what does matched sde come from ==="
grep -E "SDE|MDSS" .config | grep -v "^#" | head -8
echo "=== techpack/Kbuild ==="
cat techpack/Kbuild
cat techpack/Makefile 2>/dev/null | head -10
echo "=== 23.2 techpack/display presence ==="
ls /home/smith/kernel-200/techpack/display/ 2>/dev/null | head -8
echo SDE_DONE
