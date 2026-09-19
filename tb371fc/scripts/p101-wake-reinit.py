#!/usr/bin/env python3
# p101: after the DT ON set (sent back-to-back with zero delay between
# sleep-out 0x11 and display-on 0x29), re-send 11 -> 130ms -> 29, then read
# 0x0A inline. Evidence: good state power mode = 0x9C, wake-black = 0x0C
# (booster/status bits missing) while panel still ACKs DCS reads.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = ("\t\t\tp99_rc = dsi_panel_read_cmd_set(panel, &p99_cfg);\n"
       "\t\t\tfor (p99_i = 0; p99_i < 2; p99_i++)\n"
       "\t\t\t\tpr_info(\"P99: rbuf[%d]=0x%02x\\n\", p99_i,\n"
       "\t\t\t\t\tp99_cfg.rbuf[p99_i]);\n"
       "\t\t\tpr_info(\"P99: DCS 0x0A read rc=%d\\n\", p99_rc);\n"
       "\t\t}\n"
       "\t}\n")
new = ("\t\t\tp99_rc = dsi_panel_read_cmd_set(panel, &p99_cfg);\n"
       "\t\t\tfor (p99_i = 0; p99_i < 2; p99_i++)\n"
       "\t\t\t\tpr_info(\"P99: rbuf[%d]=0x%02x\\n\", p99_i,\n"
       "\t\t\t\t\tp99_cfg.rbuf[p99_i]);\n"
       "\t\t\tpr_info(\"P99: DCS 0x0A read rc=%d\\n\", p99_rc);\n"
       "\t\t}\n"
       "\n"
       "\t\t/* TB371FC p101: DT ON set sends sleep-out and display-on\n"
       "\t\t * back-to-back with zero delay; NT36532 needs ~120ms between\n"
       "\t\t * them, so after a real sleep-in the panel ends analog-off\n"
       "\t\t * (power mode 0x0C instead of 0x9C). Re-send with timing. */\n"
       "\t\t{\n"
       "\t\t\tu8 p101_slp[2] = {0x11, 0x00};\n"
       "\t\t\tu8 p101_dis[2] = {0x29, 0x00};\n"
       "\t\t\tu8 p101_tx = 0x0A;\n"
       "\t\t\tu8 p101_rx[2] = {0, 0};\n"
       "\t\t\tstruct mipi_dsi_msg p101_m;\n"
       "\t\t\tconst struct mipi_dsi_host_ops *p101_ops = panel->host->ops;\n"
       "\t\t\tssize_t p101_len;\n"
       "\n"
       "\t\t\tmemset(&p101_m, 0, sizeof(p101_m));\n"
       "\t\t\tp101_m.type = MIPI_DSI_DCS_SHORT_WRITE;\n"
       "\t\t\tp101_m.channel = 0;\n"
       "\t\t\tp101_m.tx_buf = p101_slp;\n"
       "\t\t\tp101_m.tx_len = 2;\n"
       "\t\t\tp101_m.flags = MIPI_DSI_MSG_USE_LPM;\n"
       "\t\t\tp101_len = p101_ops->transfer(panel->host, &p101_m);\n"
       "\t\t\tpr_info(\"P101: sleep-out len=%d\\n\", (int)p101_len);\n"
       "\t\t\tmsleep(130);\n"
       "\t\t\tmemset(&p101_m, 0, sizeof(p101_m));\n"
       "\t\t\tp101_m.type = MIPI_DSI_DCS_SHORT_WRITE;\n"
       "\t\t\tp101_m.channel = 0;\n"
       "\t\t\tp101_m.tx_buf = p101_dis;\n"
       "\t\t\tp101_m.tx_len = 2;\n"
       "\t\t\tp101_m.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;\n"
       "\t\t\tp101_len = p101_ops->transfer(panel->host, &p101_m);\n"
       "\t\t\tpr_info(\"P101: display-on len=%d\\n\", (int)p101_len);\n"
       "\t\t\tmsleep(20);\n"
       "\t\t\tp101_m.type = MIPI_DSI_DCS_READ;\n"
       "\t\t\tp101_m.tx_buf = &p101_tx;\n"
       "\t\t\tp101_m.tx_len = 1;\n"
       "\t\t\tp101_m.rx_buf = p101_rx;\n"
       "\t\t\tp101_m.rx_len = 2;\n"
       "\t\t\tp101_m.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;\n"
       "\t\t\tp101_len = p101_ops->transfer(panel->host, &p101_m);\n"
       "\t\t\tpr_info(\"P101: post-read len=%d pm=0x%02x\\n\", (int)p101_len,\n"
       "\t\t\t\t((u8 *)p101_m.rx_buf)[0]);\n"
       "\t\t}\n"
       "\t}\n")
n = src.count(old)
if n != 1:
    print(f"ANCHOR FAIL count={n}"); sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p101 applied")
