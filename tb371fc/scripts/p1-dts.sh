#!/bin/bash
cd /mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs || exit 1
for n in 0 1 2; do
  dtc -I dtb -O dts -o dtb${n}.dts dtb${n}.dtb 2>&1 | head -1
done
echo "=== dtb0 info ==="
grep -c "memory@" dtb0.dts
grep -n "reserved-memory" dtb0.dts | head -3
grep -n "qcom,msm-id\|model =" dtb0.dts | head -3
echo "=== memory reg (dtb0) ==="
grep -A4 "memory@" dtb0.dts | head -10
echo "=== #address/size ==="
grep -n "#address-cells\|#size-cells" dtb0.dts | head -4
echo DTS_DONE
