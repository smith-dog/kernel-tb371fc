#!/usr/bin/env python3
"""p217 - TASK-034 P3: apply -DWLAN_POWER_DEBUGFS via ccflags-y.

The profile defconfig sets CONFIG_WLAN_POWER_DEBUGFS := y and the Kbuild
routes it through cppflags-$(CONFIG_WLAN_POWER_DEBUGFS), but cppflags-y does
not propagate to compile flags in in-tree (staging) builds the way it does in
the Android external-module build -- wlan_hdd_sysfs.c then fails with
'incomplete type struct power_stats_response' (the type lives in sir_api.h
behind #ifdef WLAN_POWER_DEBUGFS). Bridge it through ccflags-y next to the
working INCS include. Idempotent."""

P = "/home/smith/t34/qcacld-3.0/Kbuild"
NL = chr(10)
OLD = "ccflags-y += $(INCS)"
NEW = ("ccflags-y += $(INCS)" + NL +
       "# p217: cppflags-y does not reach in-tree compiles; bridge feature flags" + NL +
       "ccflags-y += -DWLAN_POWER_DEBUGFS")
src = open(P).read()
if "p217" in src:
    print("p217: ALREADY patched")
else:
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p217: WLAN_POWER_DEBUGFS bridged via ccflags-y")
print("p217: OK")
