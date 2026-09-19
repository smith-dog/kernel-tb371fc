#!/usr/bin/env python3
# p107: one-shot soft sleep-cycle test in set_backlight, chained after p106
# (fires on next 15s P100 tick after p106_done). At boot-good 0x9c: send OFF
# set (28+10), wait 500ms, send ON set, wait 200ms, re-read 0x0A. No reset,
# no regulator cut. post=0x9c => sleep cycle harmless, wake reset/power-cut is
# the poison. post=0x0c => sleep-in itself degrades the panel.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = (
    "\t\t\t\tpr_info(\"P106: pre=0x%02x on-rc=%d post-rc=%d post=0x%02x\\n\",\n"
    "\t\t\t\t\tp106_pre, p106_rc, p100_rc, p100_cfg.rbuf[0]);\n"
    "\t\t\t}\n"
)
new = (
    "\t\t\t\tpr_info(\"P106: pre=0x%02x on-rc=%d post-rc=%d post=0x%02x\\n\",\n"
    "\t\t\t\t\tp106_pre, p106_rc, p100_rc, p100_cfg.rbuf[0]);\n"
    "\t\t\t}\n"
    "\t\t\tif (p106_done && !p107_done && p100_cfg.rbuf[0] == 0x9c) {\n"
    "\t\t\t\tint p107_rc1, p107_rc2;\n"
    "\t\t\t\tu8 p107_pre = p100_cfg.rbuf[0];\n"
    "\t\t\t\tp107_done = true;\n"
    "\t\t\t\tp107_rc1 = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_OFF);\n"
    "\t\t\t\tmsleep(500);\n"
    "\t\t\t\tp107_rc2 = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
    "\t\t\t\tmsleep(200);\n"
    "\t\t\t\tp100_rc = dsi_panel_read_cmd_set(panel, &p100_cfg);\n"
    "\t\t\t\tpr_info(\"P107: pre=0x%02x off-rc=%d on-rc=%d post-rc=%d post=0x%02x\\n\",\n"
    "\t\t\t\t\tp107_pre, p107_rc1, p107_rc2, p100_rc, p100_cfg.rbuf[0]);\n"
    "\t\t\t}\n"
)
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}")
    sys.exit(1)
src = src.replace(old, new)
old2 = "\t\tstatic bool p106_done;\n"
new2 = "\t\tstatic bool p106_done;\n\t\tstatic bool p107_done;\n"
n2 = src.count(old2)
if n2 != 1:
    print(f"ANCHOR2 FAIL count={n2}")
    sys.exit(1)
open(P, "w").write(src.replace(old2, new2))
print("p107 applied")
