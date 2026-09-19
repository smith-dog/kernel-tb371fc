#!/usr/bin/env python3
# p97: fix p93 over-broad guard in dsi_display_set_backlight.
# bl_enable is parse-false on TB371FC (no mi,feature-enabled DT prop) and only
# becomes true after the first LP1 exit (p89), so a fresh boot swallowed every
# brightness write and froze the slider. Keep only the panel-initialized check.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_display.c"
src = open(P).read()
old = "\tif (!dsi_panel_initialized(panel) || !panel->mi_cfg.bl_enable) {\n"
new = ("\t/* TB371FC p97: mi_cfg.bl_enable stays false from parse on this panel\n"
       "\t * (mi,feature-enabled DT prop absent) and only flips true after the\n"
       "\t * first LP1 exit (p89), so a fresh boot swallowed every brightness\n"
       "\t * write here and froze the slider. Hang protection only needs the\n"
       "\t * panel-initialized check. */\n"
       "\tif (!dsi_panel_initialized(panel)) {\n")
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}"); sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p97 applied")
