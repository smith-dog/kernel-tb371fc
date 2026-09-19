#!/usr/bin/env python3
"""p89 v2 — unconditional bl_enable=true at lp1 exit.

p88's type-conditional override ran before bl_config was parsed (type still
unset at lp1 time), so it never fired. This kernel is TB371FC-only; the
l3a/j11 panel-id special case has no reason to exist here.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)

START = (
    T + "//for l3a && j11" + NL
)
END = (
    T + T + "panel->mi_cfg.bl_enable = true;" + NL +
    T + "panel->mi_cfg.bl_wait_frame = false;"
)

NEW = (
    T + "/* TB371FC: the nt36532 panel reads an l3a/j11 panel_id, and the" + NL +
    T + " * l3a/j11 bl_enable=false special case here swallowed every" + NL +
    T + " * brightness set (wake stayed black). bl_ctrl_external (ktz8866a/b" + NL +
    T + " * i2c backlight ICs) is applied only through the panel bl path, so" + NL +
    T + " * keep it enabled unconditionally. */" + NL +
    T + "panel->mi_cfg.bl_enable = true;" + NL +
    T + "panel->mi_cfg.bl_wait_frame = false;"
)

src = open(P).read()
i = src.index(START)
j = src.index(END)
src = src[:i] + NEW + src[j + len(END):]
open(P, "w").write(src)
print("lp1 exit patched")
