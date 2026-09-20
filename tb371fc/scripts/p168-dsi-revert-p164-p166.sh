#!/bin/bash
# p168 (TASK-022): true revert of p164+p166 in dsi_display.c.
# techpack/ is NOT git-tracked (documented silent-revert failure mode),
# so the rollback is done by restoring the known-good pre-p164 snapshot
# taken at p164 application time (= p140 v2 shape: UNBLANK notify at the
# END of dsi_display_enable, POWERDOWN notify at disable end, no p166
# naked-reset block) and then verifying the delta against the current
# file is exactly the p164 anchor + the p166 block.
#
# Why back to UNBLANK-at-END (Xiaomi ordering: resume after the panel is
# up): n78 already proved the panel stays lit with the resume (188ms
# reflash) running at enable-end - its UNBLANK was at the END and the
# first gesture wake lit the panel fine (TOUCH-WAKE-STATUS F7). The
# "panel blacks when resume runs at panel-on" claim that motivated p164
# was disproven by n78 itself.
set -e
KV=/home/smith/android_kernel_lenovo_paladin
DSI=$KV/techpack/display/msm/dsi/dsi_display.c
GOOD=/tmp/p164-backup/dsi_display.c.p163.orig
BK=/tmp/p168-backup
LT=/mnt/d/work/code-work/logs/dt60/touch-fix-backup/before-p168

mkdir -p "$BK" "$LT"
cp -n "$DSI" "$BK/dsi_display.c.p166.orig"
cp -n "$DSI" "$LT/dsi_display.c.p166.orig"
cp -n /home/smith/android_kernel_lenovo_paladin/drivers/input/touchscreen/nt36532/nt36xxx.c "$LT/nt36xxx.c.p165.orig"

echo "--- delta BEFORE restore (current p164+p166 vs good p140v2) ---"
diff "$GOOD" "$DSI" | head -60 || true

cp "$GOOD" "$DSI"
echo "--- delta AFTER restore (must be empty) ---"
diff "$GOOD" "$DSI" && echo "DSI_RESTORE_VERIFIED"
grep -c "p166" "$DSI" || echo "p166 markers remaining: 0"
echo "P168_DONE"
