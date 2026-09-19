#!/usr/bin/env python3
"""p114 — TB371FC rate switching: keep stock 4-slot mode order, re-slot
undeliverable dfps entries to 120Hz. Replaces the p94 120Hz-only filter.

Root cause chain (2026-09-18, all VERIFIED):
- Panel nt36532 (2944x1840 3K vid, dual ctrl), stock DT declares DFPS
  VFP with list [120, 144, 60, 30], base timing vfp=26 / v_total=2050
  labeled 120Hz.
- DFPS-VFP math can only SLOW DOWN from the base: 120->60 and 120->30
  compute positive porches; 120->144 = 26 - mult_frac(2050,24,144)
  = 26 - 341 = -315 and 120->90 is negative too (dsi_display.c
  dsi_display_dfps_calc_front_porch "Invalid new_hfp calcluated-315").
- A mode whose switch can never complete wedges vendor composer
  ProcessActiveConfigChange (usleep loop, no timeout) -> SF main thread
  blocks in executeCommands -> system_server watchdog -> restart loop
  (watchdog dump 11:07 evidence; -315 value matched bit-exact).
- p94 (expose only 120Hz) did NOT stop vendor userspace from requesting
  the missing slots (n43 incident: switching to 60Hz rebooted the device
  with the filter active), so hiding modes is not enough.

Fix: keep the stock 4-slot order/count so any vendor SetActiveConfig
index keeps resolving, but relabel entries whose rate the DFPS math
cannot deliver (144/90) as plain 120Hz. All four slots become
reachable; 60/30 are genuinely delivered; 144/90 requests land on a
real 120Hz timing instead of hanging the composer.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_drm.c"
NL = chr(10)
T = chr(9)

OLD = (
    T + "for (i = 0; i < count; i++) {" + NL +
    T + T + "struct drm_display_mode *m;" + NL + NL +
    T + T + "/* TB371FC: only expose 120Hz mode. The 120<->60 mode switch" + NL +
    T + T + " * triggers ProcessActiveConfigChange() usleep loop in the" + NL +
    T + T + " * composer HAL which never completes, blocking SF main thread" + NL +
    T + T + " * in executeCommands -> watchdog kills system_server." + NL +
    T + T + " * Filtering to 120Hz-only prevents all mode switching. */" + NL +
    T + T + "if (modes[i].timing.refresh_rate != 120)" + NL +
    T + T + T + "continue;" + NL +
    NL +
    T + T + "memset(&drm_mode, 0x0, sizeof(drm_mode));"
)

NEW = (
    T + "for (i = 0; i < count; i++) {" + NL +
    T + T + "struct drm_display_mode *m;" + NL +
    T + T + "u32 mode_rr = modes[i].timing.refresh_rate;" + NL + NL +
    T + T + "/* TB371FC: DFPS-VFP (vfp 26 / vtotal 2050 @ 120Hz base) can only" + NL +
    T + T + " * slow down from the base timing; 120->60 and 120->30 are real," + NL +
    T + T + " * but 120->144 computes vfp = 26 - 341 = -315 (and 90 negative" + NL +
    T + T + " * as well), so such a switch can never complete and wedges the" + NL +
    T + T + " * composer's ProcessActiveConfigChange usleep loop -> SF main" + NL +
    T + T + " * thread blocks in executeCommands -> watchdog kills" + NL +
    T + T + " * system_server (boot loop). Keep the stock 4-slot order so" + NL +
    T + T + " * vendor SetActiveConfig indexes keep resolving: re-slot any" + NL +
    T + T + " * unreachable entry as plain 120Hz. */" + NL +
    T + T + "if (mode_rr != 60 && mode_rr != 30 && mode_rr != 120)" + NL +
    T + T + T + "modes[i].timing.refresh_rate = 120;" + NL +
    NL +
    T + T + "memset(&drm_mode, 0x0, sizeof(drm_mode));"
)

src = open(P).read()
cnt = src.count(OLD)
assert cnt == 1, f"anchor x{cnt}"
open(P, "w").write(src.replace(OLD, NEW, 1))
print("p114: 4-slot relabel patch applied to dsi_connector_get_modes")
