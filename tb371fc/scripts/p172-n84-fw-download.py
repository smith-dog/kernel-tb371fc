#!/usr/bin/env python3
# p172 (TASK-022, kernel n84): the deferred wake work must re-download the
# firmware, like stock does on EVERY resume. n83 dmesg evidence: after a
# bare bootloader_reset (swrst) the fw reboots into a half-functional
# state - config commands (0xBB/0x7F/0x79/0x7B) echo back unexecuted and
# the NEXT suspend's 0x13 arming is ignored, so double-tap detection dies
# on the cycle after any swrst-only wake. The full download re-runs the fw
# with fresh RAM and restores the healthy config path. The IRQ-time reset
# stays (panel must never enable while the IC is armed in gesture mode).
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p172-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p170.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

# a) drop the flag definition (no longer needed)
old = """/* TB371FC p170: the gesture IRQ already reset the IC out of gesture mode
 * for this wake cycle, so the deferred wake work must not reset again
 * (double reset = second-cycle touch death, TASK-022). */
static bool nvt_wake_reset_done = false;
"""
assert c.count(old) == 1, "flag: %d" % c.count(old)
c = c.replace(old, "")

# b) gesture IRQ: keep the reset, drop the flag write
old = """		nvt_bootloader_reset();
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			NVT_ERR("TB371FC: IRQ-time fw reset check FAILED\\n");
		nvt_wake_reset_done = true;
		mutex_unlock(&ts->lock);"""
new = """		nvt_bootloader_reset();
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			NVT_ERR("TB371FC: IRQ-time fw reset check FAILED\\n");
		mutex_unlock(&ts->lock);"""
assert c.count(old) == 1, "irq: %d" % c.count(old)
c = c.replace(old, new)

# c) deferred work: full firmware re-download = the stock per-wake recovery
old = """	struct nvt_ts_data *ts =
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
new = """	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);

	/* TB371FC p172: recover like stock does on EVERY resume - full
	 * firmware re-download. A bare bootloader_reset (swrst, n83) boots
	 * the flash fw into a half-functional state: config commands
	 * (0xBB/0x7F/0x79/0x7B) echo back unexecuted and the NEXT
	 * suspend's 0x13 arming is ignored, so double-tap detection dies
	 * on the cycle after any swrst-only wake (n83 dmesg). The
	 * download re-runs the fw with fresh RAM and restores the healthy
	 * config path - the IRQ-time reset above already handled the
	 * panel-enable ordering (TASK-022). */
	NVT_LOG("deferred resume start\\n");
	mutex_lock(&ts->lock);
	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME))
		NVT_ERR("TB371FC: deferred fw download FAILED\\n");
	if (nvt_check_fw_reset_state(RESET_STATE_REK))
		NVT_ERR("TB371FC: deferred fw state check FAILED\\n");"""
assert c.count(old) == 1, "deferred: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P172_DONE")
