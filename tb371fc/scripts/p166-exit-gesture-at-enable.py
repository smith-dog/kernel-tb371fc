#!/usr/bin/env python3
# p166 (TASK-022): exit the touch gesture mode at the START of the display
# enable. Data (n78+n80): the touch IC stays in gesture mode (0x13) after a
# gesture-armed blank; any display wake while the IC is mid-gesture leaves
# the panel black (gesture wakes, mouse/keyboard wakes, first power-key
# wake) and wedges the touch. A second full off/on cycle resets the IC and
# content returns. Fix: the display enable itself now resets the IC (via
# the exported nvt functions) before the panel work, so every wake - power
# key, gesture, mouse, keyboard - gets a clean panel + live touch.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "techpack/display/msm/dsi/dsi_display.c")
BK = "/tmp/p166-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "dsi_display.c.p165.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

old = """	/* TB371FC p164: notify drm_panel listeners (nt36532 wake gesture)
	 * BEFORE the panel work: the touch IC exits gesture mode and
	 * settles (188ms fw reload) while the panel is still waking, so
	 * the display pipeline never races the touch reset (TASK-022). */
	{
		int p164_blank = DRM_PANEL_BLANK_UNBLANK;
		struct drm_panel_notifier p164_ev = { .data = &p164_blank };
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EVENT_BLANK, &p164_ev);
	}
"""
new = """	/* TB371FC p166: exit the touch gesture mode and settle the IC BEFORE
	 * the panel work. In-cell: the nt36532 touch controller lives in the
	 * display DDIC; waking the panel while its touch block is mid-gesture
	 * blacks the display (gesture/mouse/keyboard wakes all affected,
	 * TASK-022). Then notify drm_panel listeners: the touch driver
	 * finishes its (deferred) resume against the lit panel. */
	{
		extern void nvt_bootloader_reset(void);
		extern int32_t nvt_check_fw_reset_state(int);
		int p166_blank = DRM_PANEL_BLANK_UNBLANK;
		struct drm_panel_notifier p166_ev = { .data = &p166_blank };

		nvt_bootloader_reset();
		nvt_check_fw_reset_state(0xA1); /* RESET_STATE_REK */
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EVENT_BLANK, &p166_ev);
	}
"""
assert c.count(old) == 1, "p164 block anchor: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P166_DONE")
