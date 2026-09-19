#!/usr/bin/env python3
# p99: DCS 0x0A (get power mode) read probe inside dsi_panel_enable, right
# after panel_initialized = true. Discriminates the wake-black bug:
# rc > 0  => panel acks DCS reads => commands reach the panel, problem is in
#            the video-stream/SDE path.
# rc <= 0 => transfer failed => commands never reach the panel (link/ULPS/
#            panel-state issue), matching the user's receiver-side hypothesis.
# Read commands auto-embed BTA in dsi_ctrl_build_cmd_pkt (dsi_ctrl.c:1104).
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = ("\telse\n"
       "\t\tpanel->panel_initialized = true;\n")
new = ("\telse {\n"
       "\t\tpanel->panel_initialized = true;\n"
       "\n"
       "\t\t/* TB371FC p99: 0x0A power-mode read probe. rc>0 => panel acks\n"
       "\t\t * DCS (video path suspect); rc<=0 => commands never reach the\n"
       "\t\t * panel (link/panel-state suspect). */\n"
       "\t\t{\n"
       "\t\t\tstruct dsi_read_config p99_cfg;\n"
       "\t\t\tstruct dsi_cmd_desc p99_desc;\n"
       "\t\t\tu8 p99_tx = 0x0A;\n"
       "\t\t\tint p99_rc, p99_i;\n"
       "\n"
       "\t\t\tmemset(&p99_cfg, 0, sizeof(p99_cfg));\n"
       "\t\t\tmemset(&p99_desc, 0, sizeof(p99_desc));\n"
       "\t\t\tp99_desc.msg.channel = 0;\n"
       "\t\t\tp99_desc.msg.type = MIPI_DSI_DCS_READ;\n"
       "\t\t\tp99_desc.msg.tx_buf = &p99_tx;\n"
       "\t\t\tp99_desc.msg.tx_len = 1;\n"
       "\t\t\tp99_desc.last_command = true;\n"
       "\t\t\tp99_cfg.is_read = true;\n"
       "\t\t\tp99_cfg.read_cmd.count = 1;\n"
       "\t\t\tp99_cfg.read_cmd.cmds = &p99_desc;\n"
       "\t\t\tp99_cfg.read_cmd.state = DSI_CMD_SET_STATE_LP;\n"
       "\t\t\tp99_cfg.cmds_rlen = 2;\n"
       "\t\t\tp99_rc = dsi_panel_read_cmd_set(panel, &p99_cfg);\n"
       "\t\t\tfor (p99_i = 0; p99_i < 2; p99_i++)\n"
       "\t\t\t\tpr_info(\"P99: rbuf[%d]=0x%02x\\n\", p99_i,\n"
       "\t\t\t\t\tp99_cfg.rbuf[p99_i]);\n"
       "\t\t\tpr_info(\"P99: DCS 0x0A read rc=%d\\n\", p99_rc);\n"
       "\t\t}\n"
       "\t}\n")
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}"); sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p99 applied")
