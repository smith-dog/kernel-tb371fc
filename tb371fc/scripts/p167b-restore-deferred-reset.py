#!/usr/bin/env python3
# p167b (TASK-022): correction to p167. Discovery while verifying the
# rebuilt Image: nvt_ts_resume is DEAD CODE since p154 (make warning
# "unused function nvt_ts_resume"; the drm_panel UNBLANK branch only
# schedules the deferred work). The every-cycle-reliable POWER-KEY wake
# recovery is therefore the deferred work itself: bootloader_reset +
# check_fw_reset_state + restore. p167 wrongly removed that reset on the
# belief that nvt_ts_resume did a full reflash - it is never called, so
# p167-as-built left NO recovery on ANY wake path (p163 failure mode).
# Fix: restore the deferred reset - the gesture path then becomes
# byte-identical to the proven power-key path, with the p160 IRQ-time
# reset (the only delta between the failing gesture path and the working
# power-key path, F9) still removed by p167 edit A.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p167-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p167.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

old = """	/* TB371FC p167: NO naked reset here. nvt_ts_resume already did the
	 * full reset+reflash (the proven recovery); a bare bootloader_reset
	 * re-resets a healthy IC for no gain - that is the wedge pattern of
	 * the p163/p165/p166 experiments (0x95, touch dead, TASK-022).
	 * Keep the state poll as instrumentation + restore runtime config. */
	NVT_LOG("deferred resume start\\n");
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);"""
new = """	/* TB371FC p167b: this reset IS the wake recovery. nvt_ts_resume is
	 * dead code since p154 (UNBLANK only schedules this work), so the
	 * proven power-key path recovers the IC exactly here: ONE
	 * bootloader_reset + check + restore, 400ms after the panel lit.
	 * p167 removed it and broke every wake path; what actually killed
	 * the second cycle was the p160 SECOND reset at the gesture IRQ
	 * (its check fails, 0xA0, and the IC wedges) - that one stays
	 * removed (TASK-022). */
	NVT_LOG("deferred resume start\\n");
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);"""
assert c.count(old) == 1, "p167 deferred block: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P167B_DONE")
