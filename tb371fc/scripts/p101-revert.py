#!/usr/bin/env python3
# revert p101 insertion (removes everything from the p101 comment to the
# end of the p99 else-block), restoring the p100-only state.
import sys
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
src = open(P).read()
start = "\n\t\t/* TB371FC p101: DT ON set sends sleep-out and display-on\n"
end = "\t\t}\n\t}\n"
i = src.find(start)
if i < 0:
    print("p101 not present"); sys.exit(0)
j = src.find(end, i)
if j < 0:
    print("end anchor not found"); sys.exit(1)
src = src[:i] + "\n\t\t}\n\t}\n" + src[j+len(end):]
open(P, "w").write(src)
print("p101 reverted")
