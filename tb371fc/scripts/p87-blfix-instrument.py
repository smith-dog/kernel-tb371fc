#!/usr/bin/env python3
"""p87 — unconditional instrumentation in dsi_panel_set_backlight."""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)
Q = chr(34)
BS = chr(92)

# 1. entry print (right after the ext_bridge_mode early return)
A1 = (
    "int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl)" + NL +
    "{" + NL +
    T + "int rc = 0;" + NL
)
N1 = (
    "int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl)" + NL +
    "{" + NL +
    T + "int rc = 0;" + NL +
    T + "pr_info(" + Q + "BLFIX: panel_set_backlight lvl=%u ext=%d bl_enable=%d type=%d" + BS + "n" + Q +
    ", bl_lvl, panel->host_config.ext_bridge_mode," + NL +
    T + T + "panel->mi_cfg.bl_enable, panel->bl_config.type);" + NL
)

# 2. external case print
A2 = (
    T + T + "extern int lcd_bl_set_led_brightness(int value);" + NL
)
N2 = (
    T + T + "pr_info(" + Q + "BLFIX: external case reached, calling ktz" + BS + "n" + Q + ");" + NL +
    T + T + "extern int lcd_bl_set_led_brightness(int value);" + NL
)

src = open(P).read()
assert src.count(A1) == 1, "A1 anchor"
assert src.count(A2) == 1, "A2 anchor"
src = src.replace(A1, N1, 1)
src = src.replace(A2, N2, 1)
open(P, "w").write(src)
print("instrumented")
