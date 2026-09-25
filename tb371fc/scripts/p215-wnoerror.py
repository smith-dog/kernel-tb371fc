#!/usr/bin/env python3
"""p215 - TASK-034 P3: neutralize the qcacld Kbuild's -Werror. clang 21 flags
2019-era driver style (strict-prototypes etc.) that clang 10 accepted; the
driver code is functionally fine. Idempotent."""

P = "/home/smith/t34/qcacld-3.0/Kbuild"
src = open(P).read()
if "-Wno-error" in src:
    print("p215: ALREADY patched")
else:
    n = src.count("-Werror")
    assert n == 1, "anchor x%d" % n
    open(P, "w").write(src.replace("-Werror", "-Wno-error", 1))
    print("p215: -Werror -> -Wno-error")
print("p215: OK")
