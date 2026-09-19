#!/bin/bash
# P1: force LF checkout of kernel tree (Windows clone had CRLF)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-fixcrlf.log 2>&1
KDIR=/home/smith/android_kernel_xiaomi_sm8250
cd $KDIR || exit 1
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -2
echo "CR lines in init/Kconfig: $(grep -c $'\r' init/Kconfig || true)"
echo "CR lines in Makefile: $(grep -c $'\r' Makefile || true)"
ls drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.h include/soc/arc/aux.h 2>&1
git status --porcelain | wc -l
echo FIX_DONE
