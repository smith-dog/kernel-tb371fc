#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-untar181.log 2>&1
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
rm -rf /home/smith/kernel-181
mkdir -p /home/smith/kernel-181
cp -a $BASE/reference/k181/.git /home/smith/kernel-181/.git
cd /home/smith/kernel-181 || exit 1
git config --global --add safe.directory /home/smith/kernel-181
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -1
echo "CR in Makefile: $(grep -c $'\r' Makefile || true)"
git log --oneline -1
ls arch/arm64/configs/vendor/ 2>/dev/null | grep kona
ls drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.h 2>/dev/null && echo AUX_OK
echo UNTAR_DONE
