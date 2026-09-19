#!/bin/bash
# Chain: rebuild 18.1 worktree in WSL -> build -> repack
cd /d/work/code-work/project/tb371fc-kernel || exit 1
echo "=== WSL: rebuild worktree from .git ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -u root -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-181b-wsl.sh
sleep 2
tail -4 /d/work/code-work/project/tb371fc-kernel/logs/p2-untar181.log

echo "=== WSL: build lineage-18.1 ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -u root -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-build181.sh
sleep 2
tail -8 /d/work/code-work/project/tb371fc-kernel/logs/p2-build181.log

echo "=== repack boot-v5 ==="
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-181 out/boot-v5-181.img out/dtbs/tail-new.bin
ls -la out/boot-v5-181.img out/Image-181
echo CHAIN_DONE
