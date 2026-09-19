#!/usr/bin/env python3
# p108: keep panel power across screen-off. Evidence (p99-p107): kernel
# reset+vreg-cut+ON set at wake always yields panel 0x0c (degraded: no
# 0x80/0x10 bits, black); ON set alone (p106) and soft sleep cycle with power
# kept (p107) both preserve 0x9c. So skip dsi_panel_power_off/on entirely:
# screen-off = OFF set only (28+10, panel sleeps, regulators+reset stay),
# wake = ON set directly (P107-proven path). Boot is unaffected: POMS path
# never calls power_on (verified: first V27TRACE power_on in p105-wake.log
# was at wake, not boot).
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()

old_on = (
    "static int dsi_panel_power_on(struct dsi_panel *panel)\n"
    "{\n"
    "\tpr_info(\"V27TRACE: dsi_panel_power_on enter\\n\");\n"
    "\tint rc = 0;\n"
    "\n"
)
new_on = (
    "/* TB371FC p108: kernel power-cut+reset+init at wake always degrades the\n"
    " * panel to 0x0c (black); soft sleep with power kept preserves 0x9c\n"
    " * (p106/p107 evidence). Skip both power_off and power_on. */\n"
    "static bool p108_keep_panel_power = true;\n"
    "\n"
    "static int dsi_panel_power_on(struct dsi_panel *panel)\n"
    "{\n"
    "\tpr_info(\"V27TRACE: dsi_panel_power_on enter\\n\");\n"
    "\tint rc = 0;\n"
    "\n"
    "\tif (p108_keep_panel_power) {\n"
    "\t\tpr_info(\"P108: keep panel power, skip power_on\\n\");\n"
    "\t\treturn 0;\n"
    "\t}\n"
    "\n"
)
n = src.count(old_on)
if n != 1:
    print(f"ANCHOR_ON FAIL count={n}")
    sys.exit(1)
src = src.replace(old_on, new_on)

old_off = (
    "static int dsi_panel_power_off(struct dsi_panel *panel)\n"
    "{\n"
    "\tint rc = 0;\n"
    "\n"
)
new_off = (
    "static int dsi_panel_power_off(struct dsi_panel *panel)\n"
    "{\n"
    "\tint rc = 0;\n"
    "\n"
    "\tif (p108_keep_panel_power) {\n"
    "\t\tpr_info(\"P108: keep panel power, skip power_off\\n\");\n"
    "\t\treturn 0;\n"
    "\t}\n"
    "\n"
)
n = src.count(old_off)
if n != 1:
    print(f"ANCHOR_OFF FAIL count={n}")
    sys.exit(1)
open(P, "w").write(src.replace(old_off, new_off))
print("p108 applied")
