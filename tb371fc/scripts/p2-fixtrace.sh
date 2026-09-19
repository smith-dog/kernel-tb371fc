#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-fixtrace.log 2>&1
cd /home/smith/kernel-200 || exit 1
echo "=== pll/Makefile head ==="
head -6 techpack/display/pll/Makefile
echo "=== patch both ==="
M=techpack/display/pll/Makefile
grep -q 'srctree)/\$(src)' $M || sed -i 's|^\(ccflags-y[[:space:]]*[:+]*=[[:space:]]*.*\)$|\1 -I\$(srctree)/\$(src)|' $M
grep -q 'srctree)/\$(src)' $M || sed -i '1i ccflags-y := -I\$(srctree)/\$(src)' $M
H=drivers/hid/Makefile
grep -q 'hid-trace' $H || echo 'CFLAGS_hid-trace.o := -I$(src)' >> $H
echo "=== verify ==="
grep -n "srctree" $M | head -2
tail -2 $H
echo FIX_DONE
