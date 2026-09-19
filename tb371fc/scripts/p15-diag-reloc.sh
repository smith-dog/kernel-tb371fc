#!/bin/bash
# Diagnose relocation types in our KPM files vs official demo (GOT hypothesis for rc=-8)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p15-diag-reloc.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
RE="${TC}readelf"
ls "$RE" >/dev/null 2>&1 && echo RE_OK || RE=aarch64-linux-gnu-readelf
echo "RE=$RE"

echo "=== toolchain default PIC/PIE setting ==="
$TCgcc -Q --help=common 2>/dev/null | grep -iE "fpic|fpie|fno-pic|fno-pie" | head -6

for f in $BASE/kpms/tb371fc-ramoops/tb371fc-ramoops.kpm \
         $BASE/kpms/tb371fc-ramoops/ramoops.o \
         $BASE/kpms/tb371fc-dump/tb371fc-dump.kpm \
         $BASE/out/demo-nsp.kpm; do
  echo ""
  echo "===== $f ====="
  ls -la $f 2>/dev/null
  echo "--- relocations ---"
  $RE -r $f 2>&1 | head -30
  echo "--- reloc type histogram ---"
  $RE -r $f 2>/dev/null | awk '{print $3}' | grep -E "^[0-9]+$" | sort -n | uniq -c
done
echo DONE
