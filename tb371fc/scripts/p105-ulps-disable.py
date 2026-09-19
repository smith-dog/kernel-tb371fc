#!/usr/bin/env python3
# p105: force-disable ULPS on TB371FC. Wake-black evidence: LP DCS reads ACK,
# panel reports NORMAL+DISPLAY (0x0c) but missing 0x90 bits; host video engine
# runs clean with no DSI errors. Suspect ULPS exit on the dual-ctrl split link
# leaves the panel's HS receive path dead. DT has qcom,ulps-enabled; override
# the parse so ULPS is never entered.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = (
    "\tpanel->ulps_feature_enabled =\n"
    "\t\tutils->read_bool(utils->data, \"qcom,ulps-enabled\");\n"
    "\n"
    "\tDSI_DEBUG(\"%s: ulps feature %s\\n\", __func__,\n"
    "\t\t(panel->ulps_feature_enabled ? \"enabled\" : \"disabled\"));\n"
)
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
    "\n"
    "\tDSI_DEBUG(\"%s: ulps feature %s\\n\", __func__,\n"
    "\t\t(panel->ulps_feature_enabled ? \"enabled\" : \"disabled\"));\n"
)
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}")
    sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p105 applied")
