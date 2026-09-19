#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-fixtrace2.log 2>&1
cd /home/smith/kernel-200 || exit 1
echo "=== before ==="
grep -n "TRACE_INCLUDE_PATH" techpack/display/pll/pll_trace.h drivers/hid/hid-trace.h
sed -i 's|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH techpack/display/pll|' techpack/display/pll/pll_trace.h
sed -i 's|define TRACE_INCLUDE_PATH \.|define TRACE_INCLUDE_PATH drivers/hid|' drivers/hid/hid-trace.h
echo "=== after ==="
grep -n "TRACE_INCLUDE_PATH" techpack/display/pll/pll_trace.h drivers/hid/hid-trace.h
echo FIX2_DONE
