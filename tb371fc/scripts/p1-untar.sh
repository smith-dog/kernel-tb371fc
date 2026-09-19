#!/bin/bash
# P1: unpack kernel source into WSL ext4 filesystem
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-untar.log 2>&1
echo START
rm -rf ~/kernel ~/android_kernel_xiaomi_sm8250
time tar -xf /mnt/d/work/code-work/project/tb371fc-kernel/kernel-src.tar -C ~/
du -sh ~/android_kernel_xiaomi_sm8250
git -C ~/android_kernel_xiaomi_sm8250 log --oneline -1
git -C ~/android_kernel_xiaomi_sm8250 branch --show-current
echo UNTAR_DONE
