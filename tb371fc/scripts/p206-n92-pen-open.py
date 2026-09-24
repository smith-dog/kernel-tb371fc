#!/usr/bin/env python3
"""p206 - TASK-033 n92 pen b3 00 gate-open (FACT-026 mandatory step).

Stock evidence (Image-b, t31/pen-gate-b.asm + BL sweep):
- dsi_panel_post_enable (file 0xf132d8) gates match_fps_pen_setting on
  strstr(saved_command_line, spinel_boe_name) || strstr(.., spinel_tianma_name)
  -- NOT on the CPHY/panel_id check our tree carries (always false here).
- dsi_panel_match_fps_pen_setting (0xf133d0) dispatches refresh
  144->tx(PEN_144HZ), 120->PEN_120HZ, 60->PEN_60HZ, 30->PEN_30HZ.
- Only live consumption point for this panel: post_enable (kickoff gate
  CPHY||panel_id && VRR is dead on stock too; mipi_reg read/write are
  debugfs-only, no runtime panel_id fill).
So n92 = restore the 144 enum/map/state entries + 144 dispatch leg + swap
the always-false gate for the panel-name gate (same gate shape as p200).
Anchored edits, assert-once, idempotent."""

D = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/techpack/display/msm/dsi"
NL = chr(10)
T = chr(9)

# --- 1) dsi_defs.h enum: PEN_144HZ before PEN_120HZ (stock order 144/120/60/30) ---
P = D + "/dsi_defs.h"
src = open(P).read()
if "DSI_CMD_SET_DISP_PEN_144HZ" in src:
    print("p206: dsi_defs.h ALREADY patched")
else:
    OLD = T + "DSI_CMD_SET_DISP_PEN_120HZ,"
    n = src.count(OLD)
    assert n == 1, "anchor1 x%d" % n
    NEW = (T + "DSI_CMD_SET_DISP_PEN_144HZ," + NL +
           T + "DSI_CMD_SET_DISP_PEN_120HZ,")
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p206: dsi_defs.h PEN_144HZ enum inserted")

# --- 2+3) dsi_panel.c prop/state maps + post_enable gate ---
P = D + "/dsi_panel.c"
src = open(P).read()
if "pen-144hz-command" in src:
    print("p206: dsi_panel.c maps ALREADY patched")
else:
    OLD = T + "\"qcom,mdss-dsi-dispparam-pen-120hz-command\","
    n = src.count(OLD)
    assert n == 1, "anchor2 x%d" % n
    NEW = (T + "\"qcom,mdss-dsi-dispparam-pen-144hz-command\"," + NL +
           T + "\"qcom,mdss-dsi-dispparam-pen-120hz-command\",")
    src = src.replace(OLD, NEW, 1)

    OLDS = T + "\"qcom,mdss-dsi-dispparam-pen-120hz-command-state\","
    n = src.count(OLDS)
    assert n == 1, "anchor3 x%d" % n
    NEWS = (T + "\"qcom,mdss-dsi-dispparam-pen-144hz-command-state\"," + NL +
            T + "\"qcom,mdss-dsi-dispparam-pen-120hz-command-state\",")
    src = src.replace(OLDS, NEWS, 1)
    open(P, "w").write(src)
    print("p206: dsi_panel.c pen-144hz map entries inserted")

if "TASK-033 n92" in src:
    print("p206: dsi_panel.c gate ALREADY patched")
else:
    OLDG = (T + "if (panel->host_config.phy_type == DSI_PHY_TYPE_CPHY || panel->mi_cfg.panel_id == 0x4C38314100420400) {" + NL +
            T + T + "rc = dsi_panel_match_fps_pen_setting(panel, panel->cur_mode);")
    n = src.count(OLDG)
    assert n == 1, "anchor4 x%d" % n
    NEWG = (T + "/* TASK-033 n92: stock (Image-b dsi_panel_post_enable) gates the" + NL +
            T + " * TP fps pen cmds on the spinel nt36532 panel names; the" + NL +
            T + " * CPHY/panel_id form was the trimmed always-false variant that" + NL +
            T + " * left the TDDI panel deaf to the 144Hz tier (FACT-026 storm). */" + NL +
            T + "if (panel->name &&" + NL +
            T + "    (strstr(panel->name, \"nt36532 tianma\") ||" + NL +
            T + "     strstr(panel->name, \"nt36532 boe\"))) {" + NL +
            T + T + "rc = dsi_panel_match_fps_pen_setting(panel, panel->cur_mode);")
    open(P, "w").write(src.replace(OLDG, NEWG, 1))
    print("p206: dsi_panel.c post_enable pen gate opened")

# --- 4) dsi_panel_mi.c 144 dispatch leg ---
P = D + "/dsi_panel_mi.c"
src = open(P).read()
if "DSI_CMD_SET_DISP_PEN_144HZ" in src:
    print("p206: dsi_panel_mi.c ALREADY patched")
else:
    OLD = (T + "/* match fps(120/60/30Hz) pen seeting cmd */" + NL +
           T + "if (adj_mode->timing.refresh_rate == 120)" + NL +
           T + T + "rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PEN_120HZ);")
    n = src.count(OLD)
    assert n == 1, "anchor5 x%d" % n
    NEW = (T + "/* match fps(120/144/60/30Hz) pen seeting cmd */" + NL +
           T + "if (adj_mode->timing.refresh_rate == 144)" + NL +
           T + T + "rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PEN_144HZ);" + NL +
           T + "else if (adj_mode->timing.refresh_rate == 120)" + NL +
           T + T + "rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_PEN_120HZ);")
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p206: dsi_panel_mi.c 144 dispatch leg added")
print("p206: OK")
