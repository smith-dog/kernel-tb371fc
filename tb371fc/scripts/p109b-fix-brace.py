#!/usr/bin/env python3
# p109b: fix brace imbalance left by p109 (one extra close after P100 probe).
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
old = (
    "\t\t\tpr_info(\"P100: DCS 0x0A read rc=%d rbuf=0x%02x 0x%02x\\n\",\n"
    "\t\t\t\tp100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t}\n"
)
new = (
    "\t\t\tpr_info(\"P100: DCS 0x0A read rc=%d rbuf=0x%02x 0x%02x\\n\",\n"
    "\t\t\t\tp100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n"
    "\t\t}\n"
    "\t}\n"
)
n = src.count(old)
if n != 1:
    print(f"FIX FAIL count={n}")
    sys.exit(1)
open(P, "w").write(src.replace(old, new))
print("p109b applied")
