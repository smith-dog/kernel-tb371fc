#!/bin/bash
cd /mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs || exit 1
ls -la
echo "=== full dtc run dtb0 ==="
dtc -I dtb -O dts -o dtb0.dts dtb0.dtb
echo "dtc rc=$?"
ls -la dtb0.dts 2>/dev/null
echo DTS2_DONE
