#!/usr/bin/env python3
"""p88 — keep bl_enable on for bl_ctrl_external panels (TB371FC fix).

The mi-dsi 'for l3a && j11' special case disables the panel backlight path
by panel_id; the TB371FC nt36532 panel hits one of those IDs, so every
brightness set is swallowed (recorded, never applied) -> wake stays dark.
bl_ctrl_external (ktz8866a/b) REQUIRES the panel bl path, so re-enable it.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)

OLD = (
    T + "if (panel->mi_cfg.panel_id == 0x4C334100420200 || panel->mi_cfg.panel_id == 0x4A323200380801)" + NL +
    T + T + "panel->mi_cfg.bl_enable = false;" + NL +
    T + "else" + NL +
    T + T + "panel->mi_cfg.bl_enable = true;"
)
NEW = (OLD + NL + NL +
    T + "/* TB371FC: the nt36532 panel reads an l3a/j11 panel_id, which" + NL +
    T + " * wrongly disables the panel bl path. bl_ctrl_external (ktz8866a/b" + NL +
    T + " * i2c backlight ICs) is applied only through this path, so keep it" + NL +
    T + " * enabled. */" + NL +
    T + "if (panel->bl_config.type == DSI_BACKLIGHT_EXTERNAL)" + NL +
    T + T + "panel->mi_cfg.bl_enable = true;")

src = open(P).read()
assert src.count(OLD) == 1, "anchor x%d" % src.count(OLD)
open(P, "w").write(src.replace(OLD, NEW))
print("patched")
