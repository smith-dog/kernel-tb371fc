#!/usr/bin/env python3
# p154 (TASK-022): defer the touch resume after gesture wake by 400ms.
# Data: the identical light resume succeeds on power-key wakes (IC settled)
# and fails on gesture wakes (resume races the IC's own gesture report).
# Fix: remove the p150 special light path (full resume always), and defer
# the resume in the UNBLANK branch via a 400ms delayed work so the IC has
# settled after its gesture report. Suspend stays synchronous.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
CPATH = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
HPATH = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.h")
BK = "/tmp/p154-backup"
os.makedirs(BK, exist_ok=True)
for p, n in ((CPATH, "nt36xxx.c.p150.orig"), (HPATH, "nt36xxx.h.p150.orig")):
    d = os.path.join(BK, n)
    if not os.path.exists(d):
        shutil.copy2(p, d)
        print("BACKUP %s" % d)

with open(CPATH, newline="") as f:
    c = f.read()

# 1. revert p150 light resume -> original full resume
old = """	/* TB371FC p150: on gesture wake the IC fw is intact (0x13 is a
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
new = """	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
		NVT_ERR("download firmware failed, ignore check fw state\\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}"""
assert c.count(old) == 1, "p150 light path anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK: light resume reverted to full resume")

# 2. drop the p150 flag lines
old = "bool nvt_gesture_flag = true;\nbool nvt_gesture_wake;\nEXPORT_SYMBOL(nvt_gesture_wake);"
new = "bool nvt_gesture_flag = true;"
assert c.count(old) == 1, "flag lines: %d" % c.count(old)
c = c.replace(old, new)
old = """		NVT_LOG("Enabled touch wakeup gesture\\n");
		/* TB371FC p150: mark gesture-mode suspend so resume skips the
		 * firmware reflash that wedges the IC and blacks the panel. */
		nvt_gesture_wake = true;"""
new = """		NVT_LOG("Enabled touch wakeup gesture\\n");"""
assert c.count(old) == 1, "suspend flag set: %d" % c.count(old)
c = c.replace(old, new)
print("OK: p150 flag removed")

# 3. deferred resume: work + wrapper; UNBLANK schedules it
old = """	} else if (event == DRM_PANEL_EVENT_BLANK) {
		if (*blank == DRM_PANEL_BLANK_UNBLANK) {
			NVT_LOG("hid-keyboard,%s,event=%lu, *blank=%d\\n",__func__ , event, *blank);
			nvt_ts_resume(&ts->client->dev);
			kb_hid_resume();"""
new = """	} else if (event == DRM_PANEL_EVENT_BLANK) {
		if (*blank == DRM_PANEL_BLANK_UNBLANK) {
			NVT_LOG("hid-keyboard,%s,event=%lu, *blank=%d\\n",__func__ , event, *blank);
			/* TB371FC p154: defer the resume 400ms - on gesture wake
			 * the IC is fresh off its gesture report; resuming
			 * immediately wedges it (touch dead until next cycle). */
			schedule_delayed_work(&ts->resume_work,
				msecs_to_jiffies(400));
			kb_hid_resume();"""
assert c.count(old) == 1, "unblank anchor: %d" % c.count(old)
c = c.replace(old, new)

# 4. the deferred resume wrapper + work init + struct member
old = "#if defined(CONFIG_DRM_PANEL)\nstatic int nvt_drm_panel_notifier_callback"
new = """#if defined(CONFIG_DRM_PANEL)
static void nvt_ts_deferred_resume_work(struct work_struct *work)
{
	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);

	NVT_LOG("deferred resume start\\n");
	nvt_ts_resume(&ts->client->dev);
}
static int nvt_drm_panel_notifier_callback"""
assert c.count(old) == 1, "callback anchor: %d" % c.count(old)
c = c.replace(old, new)

old = """	INIT_DELAYED_WORK(&nvt_hall_check_work, nvt_hall_check_func);"""
new = """	INIT_DELAYED_WORK(&nvt_hall_check_work, nvt_hall_check_func);
	INIT_DELAYED_WORK(&ts->resume_work, nvt_ts_deferred_resume_work);"""
if c.count(old) == 1:
    c = c.replace(old, new)
    print("OK: resume work INIT added (hall anchor)")
else:
    # fallback anchor: near fwu wq init
    old2 = '\tnvt_fwu_wq = alloc_workqueue("nvt_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);'
    assert c.count(old2) == 1, "no init anchor found"
    c = c.replace(old2, old2 + "\n\tINIT_DELAYED_WORK(&ts->resume_work, nvt_ts_deferred_resume_work);")
    print("OK: resume work INIT added (fwu anchor)")

with open(CPATH, "w", newline="") as f:
    f.write(c)

# 5. header: resume_work member
with open(HPATH, newline="") as f:
    h = f.read()
old_h = "	struct delayed_work nvt_fwu_work;"
new_h = "	struct delayed_work nvt_fwu_work;\n	struct delayed_work resume_work;"
assert h.count(old_h) == 1, "header member anchor: %d" % h.count(old_h)
h = h.replace(old_h, new_h)
with open(HPATH, "w", newline="") as f:
    f.write(h)
print("OK: resume_work member added")
print("P154_DONE")
