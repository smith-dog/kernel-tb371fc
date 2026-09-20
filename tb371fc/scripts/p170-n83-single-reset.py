#!/usr/bin/env python3
# p170 (TASK-022, kernel n83): exactly ONE IC reset per wake cycle, before
# the panel enables. Convergent evidence:
#   - n82: no reset until deferred (+400ms) -> panel enables while the IC
#     is armed in gesture mode with an undrained event -> panel black
#     (backlight only). Power-key wake lights fine (no pending event).
#   - n78: reset at the gesture IRQ -> panel lights on double-tap wake.
#   - n78/n79: the SECOND reset (deferred work) kills the next cycle.
#   - Goodix official gesture module (paladin-11) + Xiaomi nt36523 +
#     official Novatek resume rule: one reset per wake, before panel on.
# Changes:
#   a) gesture IRQ work: bootloader_reset + check BEFORE reporting the
#      wake key (p160 restored), sets nvt_wake_reset_done.
#   b) deferred work: resets ONLY when this wake had no gesture IRQ
#      (power key etc.); a gesture wake skips the second reset.
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p170-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p167c.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

# --- flag next to the other globals ---
old = """bool nvt_gesture_flag = true;
EXPORT_SYMBOL(nvt_gesture_flag);"""
new = """bool nvt_gesture_flag = true;
EXPORT_SYMBOL(nvt_gesture_flag);
/* TB371FC p170: the gesture IRQ already reset the IC out of gesture mode
 * for this wake cycle, so the deferred wake work must not reset again
 * (double reset = second-cycle touch death, TASK-022). */
static bool nvt_wake_reset_done = false;"""
assert c.count(old) == 1, "flag anchor: %d" % c.count(old)
c = c.replace(old, new)

# --- a) gesture IRQ: reset once, before reporting ---
old = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p167: report only - do NOT touch the IC here. The
		 * Xiaomi nt36523 driver (same family) also only reports; the
		 * IC stays in gesture mode (0x13) until nvt_ts_resume resets
		 * and reflashes it - the recovery that succeeds after every
		 * power-key wake. The p160 reset was the only kernel-side
		 * delta between the failing gesture-wake path and the working
		 * power-key path (TASK-022). */
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
new = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p170: exit gesture mode ONCE, here, BEFORE the wake
		 * key reaches the framework - the panel must never enable
		 * while the IC is armed in gesture mode with an undrained
		 * event (n82: panel black, backlight only, TASK-022). This is
		 * the single reset of this wake cycle; the deferred work
		 * skips its own reset via nvt_wake_reset_done. */
		nvt_bootloader_reset();
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			NVT_ERR("TB371FC: IRQ-time fw reset check FAILED\\n");
		nvt_wake_reset_done = true;
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
assert c.count(old) == 1, "gesture anchor: %d" % c.count(old)
c = c.replace(old, new)

# --- b) deferred work: conditional reset ---
old = """	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);

	/* TB371FC p167b: this reset IS the wake recovery. nvt_ts_resume is
	 * dead code since p154 (UNBLANK only schedules this work), so the
	 * proven power-key path recovers the IC exactly here: ONE
	 * bootloader_reset + check + restore, 400ms after the panel lit.
	 * p167 removed it and broke every wake path; what actually killed
	 * the second cycle was the p160 SECOND reset at the gesture IRQ
	 * (its check fails, 0xA0, and the IC wedges) - that one stays
	 * removed (TASK-022). */
	NVT_LOG("deferred resume start\\n");
	nvt_bootloader_reset();
	if (nvt_check_fw_reset_state(RESET_STATE_REK))
		NVT_ERR("TB371FC: deferred fw state check FAILED (wake recovery failed)\\n");
	mutex_lock(&ts->lock);"""
new = """	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);
	bool reset_needed;

	/* TB371FC p170: reset the IC out of gesture mode ONLY when this
	 * wake had no gesture IRQ (power key, ...). A gesture wake was
	 * already reset at the IRQ (nvt_wake_reset_done); resetting again
	 * here is the double reset that killed the second cycle on
	 * n78/n79 (TASK-022). */
	NVT_LOG("deferred resume start\\n");
	mutex_lock(&ts->lock);
	reset_needed = !nvt_wake_reset_done;
	nvt_wake_reset_done = false;
	mutex_unlock(&ts->lock);
	if (reset_needed) {
		nvt_bootloader_reset();
		NVT_LOG("wake reset applied (non-gesture wake)\\n");
	} else {
		NVT_LOG("wake already reset at gesture IRQ\\n");
	}
	if (nvt_check_fw_reset_state(RESET_STATE_REK))
		NVT_ERR("TB371FC: deferred fw state check FAILED (wake recovery failed)\\n");
	mutex_lock(&ts->lock);"""
assert c.count(old) == 1, "deferred anchor: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P170_DONE")
