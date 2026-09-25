#!/usr/bin/env python3
"""p221 - TASK-034 P3: raise qdf default trace levels so driver failures are
visible. set_default_trace_levels() leaves HDD/WMA/SYS at NONE (even QDF_TRACE
errors are suppressed by the print control), which blinded all diagnosis of
the hdd_wlan_startup hang. Bump the key modules to INFO. Idempotent."""

P = "/home/smith/t34/qca-wifi-host-cmn/qdf/linux/src/qdf_trace.c"
src = open(P).read()
if "p221" in src:
    print("p221: ALREADY patched")
else:
    n = 0
    for mod in ["HDD", "WMA", "SYS", "SME", "PE", "CDS", "HDD_SOFTAP", "HDD_DATA", "QDF", "WMI"]:
        old = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_NONE," % mod
        new = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_INFO, /* p221 */" % mod
        c = src.count(old)
        if c:
            src = src.replace(old, new)
            n += c
        elif mod == "QDF" or mod == "WMI":
            old2 = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_ERROR," % mod
            c2 = src.count(old2)
            assert c2 <= 1, "p221: %s x%d" % (mod, c2)
            if c2:
                src = src.replace(old2, "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_INFO, /* p221 */" % mod)
                n += c2
    open(P, "w").write(src)
    print("p221: raised %d module defaults to INFO" % n)
print("p221: OK")
