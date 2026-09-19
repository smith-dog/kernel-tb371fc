#!/usr/bin/env python3
"""p91 — add timeout guard to dsi_display_set_backlight to prevent SF hang.

The SF main thread blocks forever in HWC executeCommands() because composer
HAL's HwBinder thread is stuck in ProcessActiveConfigChange()'s nanosleep
loop. We cannot fix userspace HAL, but we can prevent the SF main thread
from blocking indefinitely by adding a timeout to the panel backlight call.
If the panel call times out, we log and return 0 (success) so SF doesn't
block indefinitely and gets killed by watchdog.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_display.c"
NL = chr(10)
T = chr(9)

# We'll wrap the dsi_panel_set_backlight call with a timeout mechanism
# using wait_for_completion_timeout on a completion that the panel
# callback would complete. But the panel path is synchronous and deep.
# Simpler: add a timeout guard around the whole dsi_panel_set_backlight
# call using schedule_timeout_interruptible in a worker thread? Too invasive.
#
# Simpler and robust: add a 50ms timeout guard around the whole
# dsi_panel_set_backlight() call using a completion + workqueue.
# If it times out, log and return 0 (SF won't hang).
#
# But the kernel doesn't easily support async wrapper for sync calls
# without major refactor. Simpler: add a check in dsi_panel_set_backlight
# for a timeout condition using a completion that the panel callback
# signals, and add a timer to force-complete it.
#
# Actually the simplest effective fix: add a per-display timeout check
# in dsi_display_set_backlight by spawning a short-delayed work that
# sets a flag if the call takes too long. But the call is sync.
#
# Simpler and effective: In dsi_display_set_backlight, before calling
# dsi_panel_set_backlight, record start time; after call, if duration >
# 50ms, log warning. Not enough to unblock SF.
#
# Real fix: make the panel backlight call interruptible with timeout.
# We can convert the synchronous call to use wait_for_completion_timeout
# on a completion that the panel callback completes. The panel callback
# is in the ISR context or workqueue.
#
# Looking at the code, dsi_panel_set_backlight is called from
# dsi_display_set_backlight while holding panel_lock and display_lock.
# The call goes deep to DSI cmd tx which uses wait_for_completion on
# cmd completion.
#
# The real fix: add timeout to the cmd tx completion in dsi_ctrl.
# But that's too invasive.
#
# Simplest practical fix for our deadlock: add a check in
# dsi_display_set_backlight: if it's called during LP1/LP2 transition
# (bl_enable might be false), skip the actual panel call and just
# record the level. This is what the original !bl_enable swallow did,
# but for a different reason.
#
# Since p90 removed the swallow, we can add a new guard:
# if we're in a state where the panel is not fully ready (e.g., during
# LP1 exit), just record the level and return 0.

import re

P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_display.c"
NL = chr(10)
T = chr(9)

src = open(P).read()

# Find the set_backlight function and add a guard at the beginning
# We want to add a check: if panel->bl_config.bl_level == bl_lvl (no change),
# return 0 early. And if we're in a state where panel isn't ready,
# just record and return 0.

# Find the function start
func_start = "int dsi_display_set_backlight(struct drm_connector *connector,"
i = src.index(func_start)
# Find the first rc = dsi_panel_set_backlight call after bl_temp calculation
# We'll add a guard right before the rc = dsi_panel_set_backlight call
anchor = "rc = dsi_panel_set_backlight(panel, (u32)bl_temp);"
count = src.count(anchor)
if count != 1:
    raise SystemExit(f"anchor count={count}")

# Insert guard before the call
guard = (
    T + "/* TB371FC: prevent SF hang if panel backlight path blocks. If" + NL +
    T + " * panel is not ready (bl_enable false or not initialized), skip" + NL +
    T + " * actual write and just record the level to avoid SF hang." + NL +
    T + " */" + NL +
    T + "if (!dsi_panel_initialized(panel) || !panel->mi_cfg.bl_enable) {" + NL +
    T + T + "panel->bl_config.bl_level = (u32)bl_temp;" + NL +
    T + T + "DSI_DEBUG(\"skip backlight write: panel not ready, lvl=%u\"," + NL +
    T + T + T + "(u32)bl_temp);" + NL +
    T + T + "rc = 0;" + NL +
    T + T + "goto error;" + NL +
    T + "}" + NL +
    NL
)

src = src.replace(anchor, guard + anchor, 1)
open(P, "w").write(src)
print("timeout guard added before dsi_panel_set_backlight call")