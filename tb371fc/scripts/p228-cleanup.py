#!/usr/bin/env python3
"""p228 - TASK-034 P3 cleanup: revert the diagnostic qdf trace level raise
(p224 patch C) back to upstream defaults — the INFO flood was a diagnostic
aid; production keeps the quiet upstream table. Also strips the patch C
block from p224-build.sh so future rebuilds stay clean."""
import re

Q = "/home/smith/t34/cmn22/qdf/linux/src/qdf_trace.c"
B = "/home/smith/t34/p224-build.sh"

s = open(Q).read()
n = 0
for mod in ["HDD", "WMA", "SYS", "SME", "PE", "CDS", "HDD_SOFTAP", "HDD_DATA"]:
    old = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_INFO, /* p224 */" % mod
    new = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_NONE," % mod
    c = s.count(old)
    if c:
        s = s.replace(old, new)
        n += c
for mod in ["QDF", "WMI"]:
    old = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_INFO, /* p224 */" % mod
    new = "[QDF_MODULE_ID_%s] = QDF_TRACE_LEVEL_ERROR," % mod
    c = s.count(old)
    if c:
        s = s.replace(old, new)
        n += c
open(Q, "w").write(s)
print("p228: qdf_trace reverted %d modules to upstream defaults" % n)

b = open(B).read()
m = re.search(r'echo "--- patch C:.*?PYEOF\n', b, re.S)
if m:
    b = b.replace(m.group(0), "")
    open(B, "w").write(b)
    print("p228: patch C block stripped from p224-build.sh")
else:
    print("p228: patch C block not found in p224-build.sh (already clean)")
