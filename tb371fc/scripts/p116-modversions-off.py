#!/usr/bin/env python3
"""p116 — turn CONFIG_MODVERSIONS off (plan B for vendor module loading).

p115 (MODVERSIONS=y) failed at load time: "wlan: disagrees about version of
symbol module_layout" — struct module layout CRC can never match stock because
our kernel carries config deltas (namespace/KSU/display) that stock lacks.
With MODVERSIONS=n the kernel skips per-symbol CRC checks entirely; combined
with CONFIG_MODULE_SIG=n (p115) and UTS_RELEASE "4.19.157-perf+" (p115), the
only remaining gap is the vermagic string token "modversions" inside each
vendor module — handled on-device by patching a /data copy of the modules
(/vendor is dm-verity read-only).

Kernel vermagic target: "4.19.157-perf+ SMP preempt mod_unload aarch64"
"""
import re

P = "/home/smith/android_kernel_lenovo_paladin/.config"
src = open(P).read()
pat = re.compile(r"^CONFIG_MODVERSIONS=.*$", re.M)
assert pat.search(src)
src = pat.sub("# CONFIG_MODVERSIONS is not set", src, count=1)
open(P, "w").write(src)
print("p116: CONFIG_MODVERSIONS disabled")
