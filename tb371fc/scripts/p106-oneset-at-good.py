#!/usr/bin/env python3
# p106: one-shot experiment in set_backlight first P100 fire. While panel is
# boot-good (0x9C, ABL init, kernel never sent ON set), resend the DT ON set
# and re-read power mode. If 0x9C -> 0x0C, the ON set itself breaks the panel
# (content/sequencing). If 0x9C stays, ON set is harmless and the breakage is
# in wake reset/power sequencing.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = (
    "\t\t\tp100_rc = dsi_panel_read_cmd_set(panel, &p100_cfg);\n"
    "\t\t\tpr_info(\"P100: DCS 0x0A read rc=%d rbuf=0x%02x 0x%02x\\n\",\n"
    "\t\t\t\tp100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n"
    "\t\t}\n"
    "\t}\n"
)
new = (
    "\t\t\tp100_rc = dsi_panel_read_cmd_set(panel, &p100_cfg);\n"
    "\t\t\tpr_info(\"P100: DCS 0x0A read rc=%d rbuf=0x%02x 0x%02x\\n\",\n"
    "\t\t\t\tp100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n"
    "\t\t\tif (!p106_done && p100_cfg.rbuf[0] == 0x9c) {\n"
    "\t\t\t\tint p106_rc;\n"
    "\t\t\t\tu8 p106_pre = p100_cfg.rbuf[0];\n"
    "\t\t\t\tp106_done = true;\n"
    "\t\t\t\tp106_rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
    "\t\t\t\tmsleep(150);\n"
    "\t\t\t\tp100_rc = dsi_panel_read_cmd_set(panel, &p100_cfg);\n"
    "\t\t\t\tpr_info(\"P106: pre=0x%02x on-rc=%d post-rc=%d post=0x%02x\\n\",\n"
    "\t\t\t\t\tp106_pre, p106_rc, p100_rc, p100_cfg.rbuf[0]);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t}\n"
)
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}")
    sys.exit(1)
src = src.replace(old, new)
old2 = (
    "\t\tstatic ktime_t p100_last;\n"
)
new2 = (
    "\t\tstatic ktime_t p100_last;\n"
    "\t\tstatic bool p106_done;\n"
)
n2 = src.count(old2)
if n2 != 1:
    print(f"ANCHOR2 FAIL count={n2}")
    sys.exit(1)
open(P, "w").write(src.replace(old2, new2))
print("p106 applied")
