#!/bin/bash
# Build + gate the printk-only probe KPM (tb371fc-probe).
# Same gate as p16: no GOT relocs, no stack_chk syms, undefined syms allow-list.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p20-probe-v2.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
RE="${TC}readelf"
NM="${TC}nm"

cd $BASE/kpms/tb371fc-probe || exit 1
TARGET_COMPILE=$TC make clean >/dev/null 2>&1
TARGET_COMPILE=$TC CFLAGS="-fno-pic -fno-pie" make 2>&1 | tail -2

f=tb371fc-probe.kpm
ls -la $f || { echo NO_ARTIFACT; exit 1; }

echo "undefined symbols:"; UNDEFS=$($NM -u $f | awk '{print $2}' | sort -u); echo "$UNDEFS"
bad=0
if echo "$UNDEFS" | grep -q "stack_chk"; then echo "GATE_FAIL: stack_chk"; bad=1; fi
if $RE -r $f | grep -aqE "GOT"; then echo "GATE_FAIL: GOT reloc:"; $RE -r $f | grep -aE "GOT" | head -5; bad=1; fi
# allow-list: only kallsyms_lookup_name and printk may be undefined
UNEXPECTED=$(echo "$UNDEFS" | grep -v -E "^(kallsyms_lookup_name|printk)$" | grep -v "^$")
if [ -n "$UNEXPECTED" ]; then echo "GATE_FAIL: unexpected undefined symbols: $UNEXPECTED"; bad=1; fi
echo "reloc types:"; $RE -r $f | grep -aoE "R_AARCH64_[A-Z0-9_]+" | sort | uniq -c

if [ $bad -eq 0 ]; then
  cp $f $BASE/out/tb371fc-probe-v2.kpm
  echo GATE_PASS
else
  echo GATE_REJECTED
  exit 1
fi
echo DONE
