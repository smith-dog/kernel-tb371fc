#!/usr/bin/env python3
# p110: temporarily restore the plain ULPS parse (inverse of p105) to test
# whether ULPS is the trigger of the dsi_ctrl IRQ death, with p108 keep-power
# unchanged. Re-apply with p105-ulps-disable.py after the experiment.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
new = (
    "\tpanel->ulps_feature_enabled =\n"
    "\t\tutils->read_bool(utils->data, \"qcom,ulps-enabled\");\n"
    "\n"
    "\t/* TB371FC p105: DT sets qcom,ulps-enabled, but after a full screen-off\n"
    "\t * the panel wakes reporting NORMAL+DISPLAY yet stays black with only\n"
    "\t * backlight; LP DCS reads ACK and the host video engine runs clean.\n"
    "\t * Suspect ULPS exit on the dual-ctrl split link kills the HS receive\n"
    "\t * path. Force ULPS off to test. */\n"
    "\tif (panel->ulps_feature_enabled) {\n"
    "\t\tpanel->ulps_feature_enabled = false;\n"
    "\t\tpr_info(\"P105: ULPS force-disabled (DT had it on)\\n\");\n"
    "\t}\n"
)
old = (
    "\tpanel->ulps_feature_enabled =\n"
    "\t\tutils->read_bool(utils->data, \"qcom,ulps-enabled\");\n"
)
n = src.count(new)
if n != 1:
    print(f"ANCHOR FAIL count={n}")
    sys.exit(1)
open(P, "w").write(src.replace(new, old))
print("p110 applied (ULPS force-off removed)")
