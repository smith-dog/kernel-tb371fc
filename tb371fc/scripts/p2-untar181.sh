#!/bin/bash
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p2-untar181.log 2>&1
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
rm -rf /home/smith/kernel-181
mkdir -p /home/smith/kernel-181
tar -xf $BASE/k181.tar -C /home/smith/kernel-181 --strip-components=1
cd /home/smith/kernel-181 || exit 1
git config core.autocrlf false
git config core.fileMode false
git reset --hard HEAD 2>&1 | tail -1
echo "CR in Makefile: $(grep -c $'\r' Makefile || true)"
ls arch/arm64/configs/vendor/ 2>/dev/null | grep kona
echo UNTAR_DONE
