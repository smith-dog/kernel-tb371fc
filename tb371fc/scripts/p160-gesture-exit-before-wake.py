#!/usr/bin/env python3
# p160 (TASK-022): exit gesture mode and settle the touch IC BEFORE the wake
# key reaches the framework. In-cell architecture: the nt36532 touch
# controller is embedded in the display DDIC; the display enable that follows
# the wake (panel 0x29 + video restart) wedges if the DDIC touch block is
# mid-gesture (panel black with backlight, fw reset check retry=51).
# Order now: gesture detected -> IC reset to normal -> report wake key ->
# framework wakes -> display enable on a settled IC.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p160-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p154.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

old = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
new = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p160: exit gesture mode and settle the IC BEFORE the
		 * wake key reaches the framework - the display enable that
		 * follows must not race an IC still in gesture mode (in-cell:
		 * the touch block lives in the display DDIC; a reset during the
		 * panel wake blacks the panel, TASK-022). */
		nvt_bootloader_reset();
		nvt_check_fw_reset_state(RESET_STATE_REK);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
assert c.count(old) == 1, "work gesture anchor: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P160_DONE")
