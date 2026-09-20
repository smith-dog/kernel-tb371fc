#!/usr/bin/env python3
# p150 (TASK-022): gesture-wake resume must not reflash the touch firmware.
# Evidence (n73): gesture wake -> nvt_ts_resume runs nvt_update_firmware
# (188ms full reflash) then nvt_check_fw_reset_state FAILS (retry=51,
# buf=0xA0) and all 4 restore commands fail - IC wedged, panel black with
# backlight. Power-key wake resume: no reflash needed, all commands succeed,
# panel fine. Fix: flag the gesture-mode suspend in nvt_ts_suspend and take
# the light resume path (bootloader_reset + fw reset check, no reflash) in
# nvt_ts_resume.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p150-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p147.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

# 1. flag storage next to nvt_gesture_flag
old = "bool nvt_gesture_flag = true;"
new = old + "\nbool nvt_gesture_wake;\nEXPORT_SYMBOL(nvt_gesture_wake);"
assert c.count(old) == 1, "flag anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK: nvt_gesture_wake flag added")

# 2. set the flag when suspend arms gesture mode (after enable_irq_wake)
old = """		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
		CTP_SPI_WRITE(ts->client, buf, 2);
		enable_irq_wake(ts->client->irq);
		NVT_LOG("Enabled touch wakeup gesture\\n");"""
new = """		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
		CTP_SPI_WRITE(ts->client, buf, 2);
		enable_irq_wake(ts->client->irq);
		NVT_LOG("Enabled touch wakeup gesture\\n");
		/* TB371FC p150: mark gesture-mode suspend so resume skips the
		 * firmware reflash that wedges the IC and blacks the panel. */
		nvt_gesture_wake = true;"""
assert c.count(old) == 1, "suspend gesture anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK: suspend sets gesture_wake flag")

# 3. resume: skip reflash on gesture wake
old = """	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
		NVT_ERR("download firmware failed, ignore check fw state\\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}"""
new = """	/* TB371FC p150: on gesture wake the IC fw is intact (0x13 is a
	 * runtime mode); the full reflash wedges the IC (fw reset check
	 * fails, restore commands fail) and blacks the panel. */
	if (nvt_gesture_wake) {
		nvt_bootloader_reset();
		nvt_check_fw_reset_state(RESET_STATE_REK);
		nvt_gesture_wake = false;
		NVT_LOG("gesture wake: light resume (no reflash)\\n");
	} else {
		if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
			NVT_ERR("download firmware failed, ignore check fw state\\n");
		} else {
			nvt_check_fw_reset_state(RESET_STATE_REK);
		}
	}"""
assert c.count(old) == 1, "resume anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK: resume light path for gesture wake")

with open(path, "w", newline="") as f:
    f.write(c)
print("P150_DONE")
