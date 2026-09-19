#!/bin/bash
# Rebuild tb371fc KPMs with pinned non-PIC flags + relocation gate.
# Root cause chain (2026-09-16): rc=-8 = "unsupported RELA relocation: 311"
# (R_AARCH64_ADR_GOT_PAGE) — loader relo.c supports no GOT types.
# rc=-2 = __stack_chk_guard not in KP ksyms -> must compile -fno-stack-protector.
# kallsyms_lookup_name & printk ARE KP-exported (kernel/base/start.c:43,46).
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p16-kpm-safe.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
TC=/home/smith/arm-gcc/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
RE="${TC}readelf"
NM="${TC}nm"
ls "$RE" >/dev/null || { echo NO_READELF; exit 1; }

GATE() { # $1=kpm file ; exits 1 on fail
  local f="$1" bad=0
  echo "--- gate: $f ---"
  echo "undefined symbols:"; $NM -u "$f" | awk '{print $2}' | sort -u
  if $NM -u "$f" | grep -q "stack_chk"; then echo "GATE_FAIL: stack_chk symbol present"; bad=1; fi
  if $RE -r "$f" | grep -aqE "GOT"; then
    echo "GATE_FAIL: GOT relocation present:"; $RE -r "$f" | grep -aE "GOT" | head -5; bad=1
  fi
  echo "reloc types:"; $RE -r "$f" | grep -aoE "R_AARCH64_[A-Z0-9_]+" | sort | uniq -c
  if [ $bad -eq 0 ]; then echo "GATE_PASS: $f"; else echo "GATE_REJECTED: $f"; fi
  return $bad
}

echo "===== rebuild tb371fc-ramoops (pinned -fno-pic -fno-pie) ====="
cd $BASE/kpms/tb371fc-ramoops || exit 1
TARGET_COMPILE=$TC make clean >/dev/null 2>&1
TARGET_COMPILE=$TC CFLAGS="-fno-pic -fno-pie" make 2>&1 | tail -2
GATE tb371fc-ramoops.kpm || { echo ABORT_RAMOOPS; exit 1; }
cp tb371fc-ramoops.kpm $BASE/out/tb371fc-ramoops-safe.kpm

echo "===== rebuild tb371fc-dump (pinned -fno-pic -fno-pie) ====="
cd $BASE/kpms/tb371fc-dump || exit 1
TARGET_COMPILE=$TC make clean >/dev/null 2>&1
TARGET_COMPILE=$TC CFLAGS="-fno-pic -fno-pie" make 2>&1 | tail -2
GATE tb371fc-dump.kpm || { echo ABORT_DUMP; exit 1; }
cp tb371fc-dump.kpm $BASE/out/tb371fc-dump-safe.kpm

echo "ALL_GATES_PASSED"
ls -la $BASE/out/*-safe.kpm
echo DONE
