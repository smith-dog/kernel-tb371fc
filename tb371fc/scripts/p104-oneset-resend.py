#!/usr/bin/env python3
# p104: inside the p102 block, after the clean sleep cycle, re-send the full
# DT ON set once the panel is in steady state. Hypothesis: the first ON set
# runs immediately after a hard reset and its FF-banked register writes are
# dropped; registers are what gates the booster (pm 0x9C good / 0x0C black).
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = ("\t\t\tp102_m.tx_buf = p102_dison;\n"
       "\t\t\tp102_m.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;\n"
       "\t\t\tp102_ops->transfer(panel->host, &p102_m);\n"
       "\t\t\tmsleep(30);\n"
       "\n")
new = ("\t\t\tp102_m.tx_buf = p102_dison;\n"
       "\t\t\tp102_m.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;\n"
       "\t\t\tp102_ops->transfer(panel->host, &p102_m);\n"
       "\t\t\tmsleep(30);\n"
       "\n"
       "\t\t\t/* TB371FC p104: re-send the full ON set in steady state -\n"
       "\t\t\t * the first run happens right after hard reset and its\n"
       "\t\t\t * register writes may be dropped. */\n"
       "\t\t\tp104_rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
       "\t\t\tpr_info(\"P104: ON-set resend rc=%d\\n\", p104_rc);\n"
       "\t\t\tmsleep(50);\n"
       "\n")
n = src.count(old)
if n != 1:
    print(f"anchor count={n}"); sys.exit(1)
src = src.replace(old, new)
old_decl = "\t\t\tint p102_rc;\n"
n = src.count(old_decl)
if n != 1:
    print(f"decl anchor count={n}"); sys.exit(1)
src = src.replace(old_decl, "\t\t\tint p102_rc, p104_rc;\n")
open(P, "w").write(src)
print("p104 applied")
