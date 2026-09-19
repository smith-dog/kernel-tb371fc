#!/usr/bin/env python3
# p109: remove diagnostic code after p108 fix was visually validated.
# - dsi_panel_enable: drop p102 msleep(60), p99 read probe, p102 sleep-cycle
#   block and p104 ON-set resend (P107 proved plain OFF->ON works; these added
#   ~600ms to every wake).
# - set_backlight: drop one-shot p106/p107 experiment blocks and their statics,
#   keep the rate-limited P100 read probe for future debugging.
# - keep p105 (ULPS force-off) and p108 (keep panel power) and V27TRACE lines.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()

# 1a. msleep(60) + comment
old = (
    "\t/* TB371FC p102: panel was just reset; give it time to boot or the\n"
    "\t * first ON-set commands are dropped. */\n"
    "\tmsleep(60);\n"
    "\trc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
)
new = "\trc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
n = src.count(old)
if n != 1:
    print(f"A1 FAIL count={n}")
    sys.exit(1)
src = src.replace(old, new)

# 1b. p99 + p102/p104 blocks (span slice)
i = src.find("\t\t/* TB371FC p99: 0x0A power-mode read probe")
end = "\t\t}\n\t}\n\n\tif (mi_cfg->gamma_update_flag) {"
j = src.find(end, i)
if i < 0 or j < 0:
    print(f"A2 FAIL i={i} j={j}")
    sys.exit(1)
src = src[:i] + end + src[j + len(end):]

# 2a. p106/p107 blocks
i2 = src.find("\t\t\tif (!p106_done && p100_cfg.rbuf[0] == 0x9c) {")
end2 = "\t\t\t}\n\t\t}\n\t}\n"
j2 = src.find(end2, i2)
if i2 < 0 or j2 < 0:
    print(f"A3 FAIL i={i2} j={j2}")
    sys.exit(1)
src = src[:i2] + end2 + src[j2 + len(end2):]

# 2b. statics
old = "\t\tstatic bool p106_done;\n\t\tstatic bool p107_done;\n"
n = src.count(old)
if n != 1:
    print(f"A4 FAIL count={n}")
    sys.exit(1)
src = src.replace(old, "")

open(P, "w").write(src)
print("p109 applied")
