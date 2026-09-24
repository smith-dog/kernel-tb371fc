#!/bin/bash
# p207-build-n92.sh — v27n92 = n91 + p206 pen b3 00 gate-open (FACT-026 mandatory:
# stock post_enable panel-name gate + 144 enum/map/state/dispatch restore) (TASK-033).
# Display-only source change; .config untouched => module_layout CRC stable,
# ksu.ko/ramdisk from the n89 Manager patch stay valid.
BASE=/mnt/d/work/code-work/project/devices/tb371fc
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7
LOG=$BASE/logs/p207-build.log
exec > "$LOG" 2>&1
echo "=== P207 BUILD START $(date) ==="
export PATH=$SNAP/bin:$PATH
cd "$KV" || { echo NO_TREE; exit 1; }

python3 tb371fc/scripts/p200-dsi-real144.py || { echo P200_FAILED; exit 1; }
python3 tb371fc/scripts/p206-n92-pen-open.py || { echo P206_FAILED; exit 1; }
echo "--- patch state ---"
grep -n "nt36532 tianma" techpack/display/msm/dsi/dsi_display.c | head -2
grep -n "mode_rr != 144" techpack/display/msm/dsi/dsi_drm.c
grep -n "DSI_CMD_SET_DISP_PEN_144HZ" techpack/display/msm/dsi/dsi_defs.h techpack/display/msm/dsi/dsi_panel_mi.c
grep -c "pen-144hz-command" techpack/display/msm/dsi/dsi_panel.c
grep -n "nt36532 boe" techpack/display/msm/dsi/dsi_panel.c

make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang      CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error      Image
MK=$?
echo "make Image exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; grep -nE "error:" "$LOG" | head -10; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n92

cd "$BASE" || { echo NO_BASE; exit 1; }
# pure = Manager patch base; kspatched = n89 live-patched boot as repack base
# (its header+ramdisk carry the ksu injection; only the kernel payload swaps).
python3 tools/repack_boot.py firmware/apatch_patched_11266_0.13.5_clte.img     out/Image-v27n92 out/boot-v27n92-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
python3 tools/repack_boot.py out/kernelsu_patched_20260924_084550.img     out/Image-v27n92 out/boot-v27n92-kspatched-flash.img out/dtbs/tail-new.bin 2>&1 | tail -2
echo "=== banner ==="
strings out/Image-v27n92 | grep -m1 "Linux version"
echo "=== md5 ==="
md5sum out/Image-v27n92 out/boot-v27n92-pure-q706.img out/boot-v27n92-kspatched-flash.img
echo "=== P207 BUILD DONE $(date) ==="
