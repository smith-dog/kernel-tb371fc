#!/bin/bash
# Chain: rebuild 18.1 worktree in WSL -> build -> repack
cd /d/work/code-work/project/tb371fc-kernel || exit 1
echo "=== clone lineage-20 (objects; worktree rebuilt in WSL) ==="
git -c core.autocrlf=false -c core.longpaths=true clone --depth 1 --single-branch --branch lineage-20 https://github.com/LineageOS/android_kernel_xiaomi_sm8250 reference/k20 2>&1 | tail -1
[ -d reference/k20/.git ] || { echo CLONE_FAILED; exit 1; }

echo "=== WSL: rebuild worktree from .git ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -u root -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-20-wsl.sh
sleep 2
tail -4 /d/work/code-work/project/tb371fc-kernel/logs/p2-untar200.log

echo "=== WSL: build lineage-20 ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -u root -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-build200.sh
sleep 2
tail -8 /d/work/code-work/project/tb371fc-kernel/logs/p2-build200.log

echo "=== repack boot-v5 ==="
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-200 out/boot-v5-20.img out/dtbs/tail-new.bin
ls -la out/boot-v5-20.img out/Image-200
echo CHAIN_DONE
