#!/usr/bin/env python3
"""p94 — only expose 120Hz mode to userspace to prevent refresh rate oscillation.

The 120<->60 mode switch triggers ProcessActiveConfigChange() in the
composer HAL which enters an usleep loop waiting for the config change
to complete. On our kernel this never completes properly -> SF main
thread blocks forever in executeCommands -> watchdog kills system_server.

Fix: only expose the 120Hz mode to userspace via DRM. Userspace never
sees 60Hz so never tries to switch. This eliminates the oscillation,
the ProcessActiveConfigChange hang, and the wake deadlock in one shot.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_drm.c"
NL = chr(10)
T = chr(9)

# Insert filter inside the mode loop in dsi_connector_get_modes
OLD = (
    T + "for (i = 0; i < count; i++) {" + NL +
    T + T + "struct drm_display_mode *m;" + NL + NL +
    T + T + "memset(&drm_mode, 0x0, sizeof(drm_mode));"
)

NEW = (
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

src = open(P).read()
cnt = src.count(OLD)
assert cnt == 1, f"anchor x{cnt}"
open(P, "w").write(src.replace(OLD, NEW, 1))
print("mode filter added to dsi_connector_get_modes")
