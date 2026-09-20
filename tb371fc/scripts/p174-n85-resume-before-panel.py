#!/usr/bin/env python3
# p174 (TASK-022, kernel n85): run the stock resume SYNCHRONOUSLY BEFORE
# the panel-on commands, on every wake.
# Evidence (n84 dmesg, black cycle): power-key wake 0.93s after suspend ->
# panel-on DSI commands sent to an IC still armed in gesture mode (0x13)
# -> commands lost, panel black (backlight only). The following deferred
# download then stuck at 0xA0 (check FAILED, restore cmds dead). The next
# cycle's 0x13 landed on a stuck IC and was dropped, so the panel-on saw a
# normal-mode IC and lit. => ANY panel enable against a gesture-armed IC
# blacks the panel; the recovery must complete BEFORE the panel-on
# commands. Stock does exactly this: resume (full fw download) runs before
# the display cmds of the next enable.
# Changes:
#   - nt36xxx.c notifier UNBLANK: call nvt_ts_resume() directly (stock
#     guard bTouchIsAwake dedupes the double notify); stop scheduling the
#     deferred work (its body stays compiled but unreachable).
#   - nt36xxx.c gesture IRQ work: report only (the recovery now runs at
#     enable start; matches Xiaomi/Goodix reference behavior).
#   - dsi_display.c: move the UNBLANK notify from the END of
#     dsi_display_enable back to the START (before the panel work), so the
#     ~190ms download + REK wait finish before any panel-on command.
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
NT = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
DSI = os.path.join(KV, "techpack/display/msm/dsi/dsi_display.c")
BK = "/tmp/p174-backup"
os.makedirs(BK, exist_ok=True)
for f in (NT, DSI):
    if not os.path.exists(os.path.join(BK, os.path.basename(f) + ".p172")):
        shutil.copy2(f, os.path.join(BK, os.path.basename(f) + ".p172"))
        print("BACKUP %s" % f)

with open(NT, newline="") as f:
    c = f.read()

# 1) gesture IRQ: report only
old = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p170: exit gesture mode ONCE, here, BEFORE the wake
		 * key reaches the framework - the panel must never enable
		 * while the IC is armed in gesture mode with an undrained
		 * event (n82: panel black, backlight only, TASK-022). This is
		 * the single reset of this wake cycle; the deferred work
		 * skips its own reset via nvt_wake_reset_done. */
		nvt_bootloader_reset();
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			NVT_ERR("TB371FC: IRQ-time fw reset check FAILED\\n");
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
new = """		nvt_ts_wakeup_gesture_report(input_id, point_data);
		/* TB371FC p174: report only. The recovery (full fw download,
		 * stock nvt_ts_resume) runs synchronously at the START of
		 * dsi_display_enable - before any panel-on command - and is
		 * what actually clears gesture mode for the panel (TASK-022). */
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;"""
assert c.count(old) == 1, "gesture: %d" % c.count(old)
c = c.replace(old, new)

# 2) notifier UNBLANK: synchronous stock resume instead of deferred work
#    (line-based splice: immune to the mixed tab/space indent here)
lines = c.split("\n")
si = next(i for i, l in enumerate(lines) if "schedule_delayed_work(&ts->resume_work," in l)
assert "msecs_to_jiffies(400));" in lines[si + 1], lines[si + 1]
ci = si - 3
assert "TB371FC p154: defer the resume" in lines[ci], lines[ci]
assert "wake the IC is fresh off" in lines[ci + 1], lines[ci + 1]
assert "resuming immediately wedges" in lines[ci + 2], lines[ci + 2]
pre = lines[si][: len(lines[si]) - len(lines[si].lstrip())]
lines[ci] = pre + "/* TB371FC p174: run the stock resume (full fw download)"
lines[ci + 1] = pre + " * synchronously, BEFORE the panel-on commands of this"
lines[ci + 2] = pre + " * enable: a panel enable against an IC still armed in"
lines[si] = pre + " * gesture mode blacks the panel (n84, TASK-022). The"
lines[si + 1] = pre + " * bTouchIsAwake guard dedupes the double notify. */"
lines.insert(si + 2, pre + "nvt_ts_resume(&ts->client->dev);")
c = "\n".join(lines)

with open(NT, "w", newline="") as f:
    f.write(c)
print("NT_DONE")

with open(DSI, newline="") as f:
    c = f.read()

# 3) move UNBLANK notify: remove from enable end
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
assert c.count(old) == 1, "enable end: %d" % c.count(old)
c = c.replace(old, new)

# 4) insert at enable start (p164 anchor)
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
	/* TB371FC p174: notify drm_panel listeners (nt36532 wake gesture)
	 * BEFORE the panel work. The touch recovery (full fw download,
	 * ~190ms) must complete BEFORE the panel-on commands: a panel
	 * enable against an IC still armed in gesture mode (0x13) blacks
	 * the panel (n84 quick power-key wake, TASK-022). */
	{
		int p174_blank = DRM_PANEL_BLANK_UNBLANK;
		struct drm_panel_notifier p174_ev = { .data = &p174_blank };
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EVENT_BLANK, &p174_ev);
	}
"""
assert c.count(anchor) == 1, "enable start: %d" % c.count(anchor)
c = c.replace(anchor, new_anchor)

with open(DSI, "w", newline="") as f:
    f.write(c)
print("DSI_DONE")
print("P174_DONE")
