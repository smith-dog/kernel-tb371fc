#!/usr/bin/env python3
# p165 (TASK-022 final): the deferred post-wake work must reset the IC out
# of gesture mode. Data: with gesture mode armed (0x13), the IC rejects all
# config commands (0xBB/0x7F/0x79/0x7B fail every cycle) and does not report
# normal touches - the touch is dead until something resets the IC. The only
# exits: the p160 reset (gesture-detected wakes) or the full resume's
# update_firmware (power-key wakes). The p163 light deferred work did
# neither, so the IC stayed in gesture mode and the touch stayed dead.
# Fix: deferred work = bootloader_reset + fw state check + restore queue +
# bTouchIsAwake=1. The reset runs ~400ms after the panel lit (backlight
# already ramping), so no display pipeline interference.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p165-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p163.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

old = """	/* TB371FC p163: NO reset and NO reflash here - the gesture IRQ work
	 * already reset the IC out of gesture mode (p160) and any IC reset
	 * at this point glitches the freshly-lit panel (black with
	 * backlight, TASK-022). Confirm fw state, restore runtime config,
	 * mark the touch awake. */
	NVT_LOG("deferred resume start\\n");
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);
	bTouchIsAwake = 1;
	mutex_unlock(&ts->lock);
#if NVT_CUST_PROC_CMD
	queue_work(nvt_fwu_wq, &ts_restore_cmd_work);
#endif
	NVT_LOG("deferred resume end\\n");"""
new = """	/* TB371FC p165: reset the IC out of gesture mode - in gesture mode
	 * (0x13) the IC rejects all config commands and reports no normal
	 * touches, so without this reset the touch stays dead after the
	 * wake (TASK-022). Runs ~400ms after the panel lit; the in-cell
	 * touch block reset does not disturb the lit display. */
	NVT_LOG("deferred resume start\\n");
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);
	bTouchIsAwake = 1;
	mutex_unlock(&ts->lock);
#if NVT_CUST_PROC_CMD
	queue_work(nvt_fwu_wq, &ts_restore_cmd_work);
#endif
	NVT_LOG("deferred resume end\\n");"""
assert c.count(old) == 1, "deferred work anchor: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P165_DONE")
