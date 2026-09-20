#!/usr/bin/env python3
# p164 (TASK-022): move the nvt resume (drm_panel UNBLANK notify) from the
# END of dsi_display_enable to the START (before the panel work). In-cell
# architecture: the touch controller lives in the display DDIC; doing the
# touch resume (188ms fw reflash + IC reset) right after the panel ON was
# glitching the freshly-lit panel (black with backlight). Doing it FIRST,
# while the panel is still asleep, is safe (same as the suspend side) and
# the panel then wakes with the touch already in normal mode.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "techpack/display/msm/dsi/dsi_display.c")
BK = "/tmp/p164-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "dsi_display.c.p163.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

# the current (p140) block at the end of dsi_display_enable
old = """	/* TB371FC p140: notify drm_panel listeners (nt36532 wake gesture)
	 * on unblank. The fb chain is NOT used - its dormant listeners
	 * (goodix fp) crash on blank (TASK-022). */
	if (rc == 0) {
		int p140_blank = DRM_PANEL_BLANK_UNBLANK;
		struct drm_panel_notifier p140_ev = { .data = &p140_blank };
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EVENT_BLANK, &p140_ev);
	}
	return rc;
}"""
new = """	return rc;
}"""
assert c.count(old) == 1, "old unblank block: %d" % c.count(old)
c = c.replace(old, new)

# insert the notify at the START of dsi_display_enable, after the early
# validity checks (right after SDE_EVT32 FUNC_ENTRY of dsi_display_enable)
anchor = """int dsi_display_enable(struct dsi_display *display)
{
	int rc = 0;
	struct dsi_display_mode *mode;

	if (!display || !display->panel) {
		DSI_ERR("Invalid params\\n");
		return -EINVAL;
	}
"""
new_anchor = anchor + """
	/* TB371FC p164: notify drm_panel listeners (nt36532 wake gesture)
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
cnt = c.count(anchor)
if cnt != 1:
    print("FAIL: enable anchor count = %d" % cnt)
    sys.exit(1)
c = c.replace(anchor, new_anchor)

with open(path, "w", newline="") as f:
    f.write(c)
print("P164_DONE: unblank notify moved to the start of dsi_display_enable")
