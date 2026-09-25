#!/usr/bin/env python3
"""p216 - TASK-034 P3: revert p213 (restore wlan_hdd_sysfs.o).

The power-stats sysfs struct (power_stats_response) is self-contained in
sir_api.h behind #ifdef WLAN_POWER_DEBUGFS, and the profile defconfig already
sets CONFIG_WLAN_POWER_DEBUGFS := y (Kbuild:2177 cppflags gate). The earlier
drop was based on the wrong cmn tree; with the era-matched cmn the driver
compiles complete. Idempotent."""

P = "/home/smith/t34/qcacld-3.0/Kbuild"
src = open(P).read()
if "# p213" not in src:
    print("p216: p213 drop not present, nothing to revert")
else:
    OLD = ("# p213: power-stats sysfs dropped (needs wlan-platform cnss struct," + chr(10) +
           "# absent in this staging trio; restore when platform repo is paired)" + chr(10) +
           "# HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs.o")
    NEW = "HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs.o"
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p216: wlan_hdd_sysfs.o restored")
print("p216: OK")
