#!/bin/bash
# Build the tb371fc-ramoops KPM
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p12-kpm.log 2>&1
echo START
cd /mnt/d/work/code-work/project/tb371fc-kernel/kpms/tb371fc-ramoops || exit 1
export TARGET_COMPILE=aarch64-linux-gnu-
make clean >> /dev/null 2>&1
make 2>&1 | tail -8
ls -la tb371fc-ramoops.kpm 2>/dev/null && echo KPM_BUILD_OK || echo KPM_BUILD_FAILED
echo KPM_DONE
