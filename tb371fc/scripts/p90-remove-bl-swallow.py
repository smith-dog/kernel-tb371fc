#!/usr/bin/env python3
"""p90 — remove the !bl_enable swallow in dsi_panel_set_backlight.

dsi_panel_parse_mi_config() gates on the Xiaomi-only DT property
"mi,feature-enabled", which is absent from the TB371FC panel DTB; it
returns before ever setting mi_cfg->bl_enable = true, so bl_enable stays
false (kzalloc zero) forever and every brightness set is discarded here.
bl_enable has no meaning for this panel - always apply the backlight.
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)

OLD = (
    T + "}else if (!panel->mi_cfg.bl_enable) {" + NL +
    T + T + "mi_cfg->last_bl_level = bl_lvl;" + NL +
    T + T + "if (bl_lvl)" + NL +
    T + T + T + "mi_cfg->last_nonzero_bl_level = bl_lvl;" + NL +
    T + T + "return rc;" + NL +
    T + "}"
)
NEW = (
    T + "}" + NL +
    T + "/* TB371FC: the mi,feature-enabled gate makes mi_cfg->bl_enable stay" + NL +
    T + " * false forever (parse bails early), which used to discard every" + NL +
    T + " * brightness set right here (wake stayed black). The flag is" + NL +
    T + " * meaningless for this panel - fall through and always apply. */"
)

src = open(P).read()
assert src.count(OLD) == 1, "anchor x%d" % src.count(OLD)
open(P, "w").write(src.replace(OLD, NEW))
print("swallow removed")
