#!/usr/bin/env python3
"""p208 - TASK-033 n93: open the sde_encoder_kickoff pen path (runtime
b3 pen re-send at VRR mode switches).

Stock evidence (Image-b):
- sde_encoder_kickoff+0xc58 bl dsi_panel_match_fps_pen_setting; the gate
  before it (file 0xe85fa0..0xe85fd4) = dsi_mode_flags BIT(4)
  (DSI_MODE_FLAG_VRR) && strstr(saved_command_line, spinel_boe/tianma
  nt36532 names) -- NOT the CPHY/panel_id form our fw474 tree carries.
- Timing: kickoff copies bridge->dsi_mode to a local adj_mode BEFORE
  dsi_conn_post_kickoff applies the DFPS porch update and clears VRR from
  the bridge copy, so the pen fires exactly once on the switch commit.
n92 proved the runtime-switch storm (FACT-026): our gate is always-false,
panel never told about 144. n93 swaps both gate clauses to the stock
panel-name form. Anchored, assert-once, idempotent."""

P = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/techpack/display/msm/sde/sde_encoder.c"
NL = chr(10)
T = chr(9)
GATE_OLD = "dsi_display->panel->host_config.phy_type == DSI_PHY_TYPE_CPHY || dsi_display->panel->mi_cfg.panel_id == 0x4C38314100420400"
GATE_NEW = ("dsi_display->panel->name &&" + NL +
            T + T + T + "    (strstr(dsi_display->panel->name, \"nt36532 tianma\") ||" + NL +
            T + T + T + "     strstr(dsi_display->panel->name, \"nt36532 boe\"))")

src = open(P).read()
n = src.count(GATE_OLD)
if n == 0:
    assert "nt36532 tianma" in src, "p208: no anchor and no prior patch"
    print("p208: sde_encoder.c ALREADY patched")
else:
    assert n == 2, "p208: gate clause x%d (expect 2)" % n
    src = src.replace(GATE_OLD, GATE_NEW)
    # stock-parity comment at the pen call site only
    OLD2 = (T + "if (dsi_display && dsi_display->panel" + NL +
            T + T + "&& (dsi_display->panel->name &&" + NL +
            T + T + T + "    (strstr(dsi_display->panel->name, \"nt36532 tianma\") ||" + NL +
            T + T + T + "     strstr(dsi_display->panel->name, \"nt36532 boe\")))" + NL +
            T + T + "&& adj_mode.dsi_mode_flags & DSI_MODE_FLAG_VRR) {")
    assert src.count(OLD2) == 1, "p208: site2 anchor lost"
    NEW2 = (T + "/* TASK-033 n93: stock (Image-b sde_encoder_kickoff+0xc58) gates the" + NL +
            T + " * TP fps pen on the spinel nt36532 panel names; kickoff sampled" + NL +
            T + " * adj_mode before post_kickoff clears VRR, so the pen fires once" + NL +
            T + " * per VRR switch commit (n92 storm fix, runtime leg). */" + NL +
            OLD2)
    src = src.replace(OLD2, NEW2, 1)
    open(P, "w").write(src)
    print("p208: sde_encoder.c both pen gates opened (name form)")
print("p208: OK")
