#!/usr/bin/env python3
# p100: rate-limited DCS 0x0A read in dsi_panel_set_backlight (once per 15s).
# Gives power-mode byte in known-good state (boot/desktop) and in wake-black
# state for comparison. 0x0C seen at wake = DISPLAY_ON|NORMAL, SLEEP clear,
# BOOSTER(0x80) clear. If good state shows 0x8C/0x9C (booster set) the panel
# bias/booster condition is the blocker; if identical, video path is suspect.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = ("\tint rc = 0;\n"
       "\tpr_info(\"BLFIX: panel_set_backlight lvl=%u ext=%d bl_enable=%d type=%d\\n\", bl_lvl, panel->host_config.ext_bridge_mode,\n")
new = ("\tint rc = 0;\n"
       "\t{\n"
       "\t\tstatic ktime_t p100_last;\n"
       "\t\tktime_t p100_now = ktime_get();\n"
       "\t\tif (ktime_ms_delta(p100_now, p100_last) >= 15000) {\n"
       "\t\t\tstruct dsi_read_config p100_cfg;\n"
       "\t\t\tstruct dsi_cmd_desc p100_desc;\n"
       "\t\t\tu8 p100_tx = 0x0A;\n"
       "\t\t\tint p100_rc;\n"
       "\n"
       "\t\t\tp100_last = p100_now;\n"
       "\t\t\tmemset(&p100_cfg, 0, sizeof(p100_cfg));\n"
       "\t\t\tmemset(&p100_desc, 0, sizeof(p100_desc));\n"
       "\t\t\tp100_desc.msg.channel = 0;\n"
       "\t\t\tp100_desc.msg.type = MIPI_DSI_DCS_READ;\n"
       "\t\t\tp100_desc.msg.tx_buf = &p100_tx;\n"
       "\t\t\tp100_desc.msg.tx_len = 1;\n"
       "\t\t\tp100_desc.last_command = true;\n"
       "\t\t\tp100_cfg.is_read = true;\n"
       "\t\t\tp100_cfg.read_cmd.count = 1;\n"
       "\t\t\tp100_cfg.read_cmd.cmds = &p100_desc;\n"
       "\t\t\tp100_cfg.read_cmd.state = DSI_CMD_SET_STATE_LP;\n"
       "\t\t\tp100_cfg.cmds_rlen = 2;\n"
       "\t\t\tp100_rc = dsi_panel_read_cmd_set(panel, &p100_cfg);\n"
       "\t\t\tpr_info(\"P100: DCS 0x0A read rc=%d rbuf=0x%02x 0x%02x\\n\",\n"
       "\t\t\t\tp100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n"
       "\t\t}\n"
       "\t}\n"
       "\tpr_info(\"BLFIX: panel_set_backlight lvl=%u ext=%d bl_enable=%d type=%d\\n\", bl_lvl, panel->host_config.ext_bridge_mode,\n")
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}"); sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p100 applied")
