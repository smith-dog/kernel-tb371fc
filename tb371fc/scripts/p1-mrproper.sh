#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-mrproper.log 2>&1
cd /home/smith/android_kernel_xiaomi_sm8250 || exit 1
make mrproper 2>&1 | tail -2
echo MRPROPER_DONE
