#!/bin/bash
# Full chain: clone lineage-18.1 -> pack -> untar in WSL -> build -> repack boot image
cd /d/work/code-work/project/tb371fc-kernel || exit 1
echo "=== clone lineage-18.1 ==="
git clone --depth 1 --single-branch --branch lineage-18.1 https://github.com/LineageOS/android_kernel_xiaomi_sm8250 reference/k181 2>&1 | tail -2
[ -f reference/k181/Makefile ] || { echo CLONE_FAILED; exit 1; }

echo "=== tar for WSL transfer ==="
tar -cf k181.tar -C reference k181 && ls -la k181.tar

echo "=== WSL untar + clean ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-untar181.sh
sleep 2
tail -3 /d/work/code-work/project/tb371fc-kernel/logs/p2-untar181.log

echo "=== WSL build ==="
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -u root -- bash /mnt/d/work/code-work/project/tb371fc-kernel/scripts/p2-build181.sh
sleep 2
tail -6 /d/work/code-work/project/tb371fc-kernel/logs/p2-build181.log

echo "=== repack boot-v5 ==="
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-181 out/boot-v5-181.img out/dtbs/tail-new.bin
ls -la out/boot-v5-181.img out/Image-181
echo CHAIN_DONE
