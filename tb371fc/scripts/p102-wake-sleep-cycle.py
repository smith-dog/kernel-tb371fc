#!/usr/bin/env python3
# p102: two changes in dsi_panel_enable:
#  1. msleep(60) before sending the ON set - the panel was just hard-reset
#     by dsi_panel_reset and drops early commands.
#  2. after the ON set, run a clean timed sleep cycle
#     (10 -> 150ms -> 11 -> 150ms -> 29) then read 0x0A via the proven
#     dsi_panel_read_cmd_set machinery. Evidence so far: boot-good pm=0x9C,
#     wake-black pm=0x0C, panel ACKs reads, 11/29 re-send with 130ms gap did
#     not recover it.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()

# 1. strip the p101 block (comment through its closing braces)
start = "\n\t\t/* TB371FC p101: DT ON set sends sleep-out and display-on\n"
end = "\t\t}\n\t}\n"
i = src.find(start)
if i >= 0:
    j = src.find(end, i)
    if j < 0:
        print("p101 end not found"); sys.exit(1)
    src = src[:i] + "\t}\n" + src[j+len(end):]
    print("p101 block removed")
else:
    print("p101 block not present, continuing")

# 2. msleep before ON set
old_on = "\trc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);\n"
n = src.count(old_on)
if n != 1:
    print(f"ON anchor count={n}"); sys.exit(1)
src = src.replace(old_on,
    "\t/* TB371FC p102: panel was just reset; give it time to boot or the\n"
    "\t * first ON-set commands are dropped. */\n"
    "\tmsleep(60);\n"
    + old_on)

# 3. insert p102 block after the p99 probe (end of p99 block = "\t\t}\n\t}\n"
#    right after the P99 pr_info)
old_p99 = ('\t\t\tpr_info("P99: DCS 0x0A read rc=%d\\n", p99_rc);\n'
           '\t\t}\n'
           '\t}\n')
p102 = ('\t\t\tpr_info("P99: DCS 0x0A read rc=%d\\n", p99_rc);\n'
        '\t\t}\n'
        '\n'
        '\t\t/* TB371FC p102: run a clean timed sleep cycle after the ON set\n'
        '\t\t * (10 -> 150ms -> 11 -> 150ms -> 29) so the panel internal\n'
        '\t\t * sequencer and booster come up with proper timing, then read\n'
        '\t\t * power mode. Boot-good reads 0x9C, wake-black reads 0x0C. */\n'
        '\t\t{\n'
        '\t\t\tu8 p102_slpin[2] = {0x10, 0x00};\n'
        '\t\t\tu8 p102_slpout[2] = {0x11, 0x00};\n'
        '\t\t\tu8 p102_dison[2] = {0x29, 0x00};\n'
        '\t\t\tu8 p102_tx = 0x0A;\n'
        '\t\t\tstruct dsi_read_config p102_cfg;\n'
        '\t\t\tstruct dsi_cmd_desc p102_rd;\n'
        '\t\t\tstruct mipi_dsi_msg p102_m;\n'
        '\t\t\tconst struct mipi_dsi_host_ops *p102_ops = panel->host->ops;\n'
        '\t\t\tint p102_rc;\n'
        '\n'
        '\t\t\tmemset(&p102_m, 0, sizeof(p102_m));\n'
        '\t\t\tp102_m.channel = 0;\n'
        '\t\t\tp102_m.tx_len = 2;\n'
        '\t\t\tp102_m.flags = MIPI_DSI_MSG_USE_LPM;\n'
        '\t\t\tp102_m.type = MIPI_DSI_DCS_SHORT_WRITE;\n'
        '\t\t\tp102_m.tx_buf = p102_slpin;\n'
        '\t\t\tp102_ops->transfer(panel->host, &p102_m);\n'
        '\t\t\tmsleep(150);\n'
        '\t\t\tp102_m.tx_buf = p102_slpout;\n'
        '\t\t\tp102_ops->transfer(panel->host, &p102_m);\n'
        '\t\t\tmsleep(150);\n'
        '\t\t\tp102_m.tx_buf = p102_dison;\n'
        '\t\t\tp102_m.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;\n'
        '\t\t\tp102_ops->transfer(panel->host, &p102_m);\n'
        '\t\t\tmsleep(30);\n'
        '\n'
        '\t\t\tmemset(&p102_cfg, 0, sizeof(p102_cfg));\n'
        '\t\t\tmemset(&p102_rd, 0, sizeof(p102_rd));\n'
        '\t\t\tp102_rd.msg.channel = 0;\n'
        '\t\t\tp102_rd.msg.type = MIPI_DSI_DCS_READ;\n'
        '\t\t\tp102_rd.msg.tx_buf = &p102_tx;\n'
        '\t\t\tp102_rd.msg.tx_len = 1;\n'
        '\t\t\tp102_rd.last_command = true;\n'
        '\t\t\tp102_cfg.is_read = true;\n'
        '\t\t\tp102_cfg.read_cmd.count = 1;\n'
        '\t\t\tp102_cfg.read_cmd.cmds = &p102_rd;\n'
        '\t\t\tp102_cfg.read_cmd.state = DSI_CMD_SET_STATE_LP;\n'
        '\t\t\tp102_cfg.cmds_rlen = 2;\n'
        '\t\t\tp102_rc = dsi_panel_read_cmd_set(panel, &p102_cfg);\n'
        '\t\t\tpr_info("P102: post-cycle read rc=%d pm=0x%02x\\n",\n'
        '\t\t\t\tp102_rc, p102_cfg.rbuf[0]);\n'
        '\t\t}\n'
        '\t}\n')
n = src.count(old_p99)
if n != 1:
    print(f"p99-end anchor count={n}"); sys.exit(1)
src = src.replace(old_p99, p102)
open(P, "w").write(src)
print("p102 applied")
