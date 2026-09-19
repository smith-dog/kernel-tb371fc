#!/usr/bin/env python3
# p109c: fix extra close brace in dsi_panel_enable else block left by p109.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = (
    "\telse {\n"
    "\t\tpanel->panel_initialized = true;\n"
    "\n"
    "\t\t}\n"
    "\t}\n"
    "\n"
    "\tif (mi_cfg->gamma_update_flag) {\n"
)
new = (
    "\telse {\n"
    "\t\tpanel->panel_initialized = true;\n"
    "\t}\n"
    "\n"
    "\tif (mi_cfg->gamma_update_flag) {\n"
)
n = src.count(old)
if n != 1:
    print(f"FIX FAIL count={n}")
    sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p109c applied")
