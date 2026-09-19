#!/usr/bin/env python3
"""p84 — repair the ICL vote instrumentation printk string literal in battery.c."""
P = "/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/battery.c"
NL = chr(10)
BSN = chr(92) + "n"   # backslash-n as C escape
TAB = chr(9)

broken = (
    TAB + 'pr_info("battery: ICL vote client=%s icl=%d comm=%s' + NL +
    '",' + NL +
    TAB + TAB + 'client ? client : "NULL", icl_ua, current->comm);'
)
fixed = (
    TAB + 'pr_info("battery: ICL vote client=%s icl=%d comm=%s' + BSN + '",' + NL +
    TAB + TAB + 'client ? client : "NULL", icl_ua, current->comm);'
)

src = open(P).read()
if fixed in src:
    print("already fixed")
elif broken in src:
    open(P, "w").write(src.replace(broken, fixed))
    print("fixed")
else:
    raise SystemExit("anchor not found")
