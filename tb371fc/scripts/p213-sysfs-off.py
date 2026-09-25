#!/usr/bin/env python3
"""p213 - TASK-034 P3: drop wlan_hdd_sysfs.o from the CLO qcacld build.

wlan_hdd_sysfs.c carries the power-stats sysfs node whose
'struct power_stats_response' is provided by the wlan PLATFORM repo's cnss2
(cnss.h) -- a fourth repo we do not vendor; the in-tree Lenovo cnss2 does not
define it (compile fails: incomplete type). The node is debug-only and its
create/destroy are not wired from wlan_hdd_main.c, so the object is removed
until the platform repo is paired (P4+). Idempotent."""

P = "/home/smith/t34/qcacld-3.0/Kbuild"
NL = chr(10)
OLD = "HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs.o"
src = open(P).read()
if "# p213" in src:
    print("p213: Kbuild ALREADY patched")
else:
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    new = ("# p213: power-stats sysfs dropped (needs wlan-platform cnss struct," + NL +
           "# absent in this staging trio; restore when platform repo is paired)" + NL +
           "# " + OLD)
    open(P, "w").write(src.replace(OLD, new, 1))
    print("p213: wlan_hdd_sysfs.o removed from HDD_OBJS")
print("p213: OK")
