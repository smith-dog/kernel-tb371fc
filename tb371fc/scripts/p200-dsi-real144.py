#!/usr/bin/env python3
"""p200 - TASK-033 real-144 port (stock-channel replica, read of
notes/NOTES-tb371fc-144hz-clockup-reverse.md section 4):
1) dsi_display.c dsi_display_get_dfps_timing: add the stock private 144Hz
   special branch (wire-domain hfp=146 / vfp=26, constant pixel clock),
   gated on the spinel nt36532 panel names + non-zero curr rate so only
   the build/switch paths fire (same gating shape as stock).
2) dsi_drm.c dsi_connector_get_modes: stop re-slotting the 144 entry to
   120 (p114 era); it is now genuinely deliverable.
Anchored edits, assert-once, idempotent."""
import sys

D = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/techpack/display/msm/dsi"
NL = chr(10)
T = chr(9)

# --- 1) dsi_display.c special branch ---
P = D + "/dsi_display.c"
src = open(P).read()
if "nt36532 tianma" in src:
    print("p200: dsi_display.c ALREADY patched")
else:
    OLD1 = (T + "if (!display || !adj_mode) {" + NL +
            T + T + "DSI_ERR(\"Invalid params\\n\");" + NL +
            T + T + "return -EINVAL;" + NL +
            T + "}" + NL +
            T + "m_ctrl = display->ctrl[display->clk_master_idx].ctrl;")
    n1 = src.count(OLD1)
    assert n1 == 1, "anchor1 x%d" % n1
    NEW1 = (T + "if (!display || !adj_mode) {" + NL +
            T + T + "DSI_ERR(\"Invalid params\\n\");" + NL +
            T + T + "return -EINVAL;" + NL +
            T + "}" + NL +
            T + "if (adj_mode->timing.refresh_rate == 144 &&" + NL +
            T + T + T + "display->panel &&" + NL +
            T + T + T + "(strstr(display->panel->name, \"nt36532 tianma\") ||" + NL +
            T + T + T + " strstr(display->panel->name, \"nt36532 boe\"))) {" + NL +
            T + T + "/* TB371FC TASK-033: port of the stock private 144Hz channel" + NL +
            T + T + " * (Image-b get_dfps_timing special branch): constant pixel" + NL +
            T + T + " * clock, wire-domain hfp narrowed 402->146, vfp stays at" + NL +
            T + T + " * the 26 base. Vanilla VFP math can only slow down" + NL +
            T + T + " * (120->144 computes -315) and a switch that never" + NL +
            T + T + " * completes wedges the vendor composer. */" + NL +
            T + T + "adj_mode->timing.h_front_porch = 146;" + NL +
            T + T + "adj_mode->timing.v_front_porch = 26;" + NL +
            T + T + "return 0;" + NL +
            T + "}" + NL +
            T + "m_ctrl = display->ctrl[display->clk_master_idx].ctrl;")
    open(P, "w").write(src.replace(OLD1, NEW1, 1))
    print("p200: dsi_display.c special branch inserted")

# --- 2) dsi_drm.c relabel pass-through for 144 ---
P = D + "/dsi_drm.c"
src = open(P).read()
if "mode_rr != 144" in src:
    print("p200: dsi_drm.c ALREADY patched")
else:
    OLD2 = (T + T + "if (mode_rr != 60 && mode_rr != 30 && mode_rr != 120)" + NL +
            T + T + T + "modes[i].timing.refresh_rate = 120;")
    n2 = src.count(OLD2)
    assert n2 == 1, "anchor2 x%d" % n2
    NEW2 = (T + T + "if (mode_rr != 60 && mode_rr != 30 && mode_rr != 120 &&" + NL +
            T + T + "    mode_rr != 144)" + NL +
            T + T + T + "modes[i].timing.refresh_rate = 120;")
    src = src.replace(OLD2, NEW2, 1)

    OLD3 = (T + T + " * unreachable entry as plain 120Hz. */")
    n3 = src.count(OLD3)
    assert n3 == 1, "anchor3 x%d" % n3
    NEW3 = (T + T + " * unreachable entry as plain 120Hz. Since TASK-033 the 144Hz" + NL +
            T + T + " * tier carries real wire porches (dsi_display_get_dfps_timing" + NL +
            T + T + " * stock-channel branch), so it is exposed unmodified. */")
    src = src.replace(OLD3, NEW3, 1)
    open(P, "w").write(src)
    print("p200: dsi_drm.c 144 relabel removed")
print("p200: OK")
