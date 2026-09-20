#!/usr/bin/env python3
# p167 (TASK-022): remove every naked reset from the gesture-wake path,
# leaving nvt_ts_resume's full reset+reflash as the ONLY IC recovery -
# the structure of the proven-working power-key wake path and of the
# Xiaomi nt36523 reference driver (same Novatek family):
#   suspend: 0x13 arm -> double-tap IRQ: report key only, IC untouched ->
#   panel on -> nvt_ts_resume: nvt_update_firmware (incl. bootloader
#   reset + 245KB download) + check_fw_reset_state.
# Data behind each removal:
#   - p160 reset at gesture IRQ: the only kernel-side delta between the
#     failing gesture-wake path and the succeeding power-key path (F9);
#     its check fails (retry=51, 0xA0) and leaves the resume download to
#     race a half-booted IC.
#   - p165 naked reset in deferred work (+400ms): same bare
#     bootloader_reset-without-download pattern that wedged the IC in
#     the p163/p166 experiments (0x95, touch dead).
# Kept: p133 flag default, p142 active_panel, p144/p146/p149 wake-key
# device, p154 deferred-work skeleton (check-only + restore queue).
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p167-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p165.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

# --- edit A: revert p160 (report only, no IC touching at gesture IRQ) ---
old = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p160: exit gesture mode and settle the IC BEFORE the
		 * wake key reaches the framework - the display enable that
		 * follows must not race an IC still in gesture mode (in-cell:
		 * the touch block lives in the display DDIC; a reset during the
		 * panel wake blacks the panel, TASK-022). */
		nvt_bootloader_reset();
		nvt_check_fw_reset_state(RESET_STATE_REK);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
new = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p167: report only - do NOT touch the IC here. The
		 * Xiaomi nt36523 driver (same family) also only reports; the
		 * IC stays in gesture mode (0x13) until nvt_ts_resume resets
		 * and reflashes it - the recovery that succeeds after every
		 * power-key wake. The p160 reset was the only kernel-side
		 * delta between the failing gesture-wake path and the working
		 * power-key path (TASK-022). */
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
assert c.count(old) == 1, "p160 block: %d" % c.count(old)
c = c.replace(old, new)

# --- edit B: deferred work loses its naked reset (check-only + restore) ---
old = """	/* TB371FC p165: reset the IC out of gesture mode - in gesture mode
	 * (0x13) the IC rejects all config commands and reports no normal
	 * touches, so without this reset the touch stays dead after the
	 * wake (TASK-022). Runs ~400ms after the panel lit; the in-cell
	 * touch block reset does not disturb the lit display. */
	NVT_LOG("deferred resume start\\n");
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);"""
new = """	/* TB371FC p167: NO naked reset here. nvt_ts_resume already did the
	 * full reset+reflash (the proven recovery); a bare bootloader_reset
	 * re-resets a healthy IC for no gain - that is the wedge pattern of
	 * the p163/p165/p166 experiments (0x95, touch dead, TASK-022).
	 * Keep the state poll as instrumentation + restore runtime config. */
	NVT_LOG("deferred resume start\\n");
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);"""
assert c.count(old) == 1, "p165 block: %d" % c.count(old)
c = c.replace(old, new)

# --- edit C: make the resume-path reset-state verdict explicit ---
old = """	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
		NVT_ERR("download firmware failed, ignore check fw state\\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}"""
new = """	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
		NVT_ERR("download firmware failed, ignore check fw state\\n");
	} else {
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			NVT_ERR("TB371FC p167: fw reset state check FAILED after update\\n");
	}"""
assert c.count(old) == 1, "resume check block: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P167_DONE")
