#!/usr/bin/env python3
"""p116 on-device helper — patch vendor module vermagic strings for the
MODVERSIONS-off custom kernel (4.19.157-perf+).

Old vermagic (stock build):  4.19.157-perf+ SMP preempt mod_unload modversions aarch64
New vermagic (our kernel):   4.19.157-perf+ SMP preempt mod_unload aarch64

Same-length in-place replacement (zero padding) keeps ELF section offsets
intact. Signature blob at EOF is left alone (kernel sig checking is off).
"""
import sys, os, glob

OLD = b"4.19.157-perf+ SMP preempt mod_unload modversions aarch64\x00"
NEW = b"4.19.157-perf+ SMP preempt mod_unload aarch64\x00"
PAD = len(OLD) - len(NEW)
assert PAD > 0

def patch(path):
    data = open(path, "rb").read()
    n = data.count(OLD)
    if n == 0:
        return "no-vm"
    data = data.replace(OLD, NEW + b"\x00" * PAD)
    open(path, "wb").write(data)
    return f"ok x{n}"

for p in sys.argv[1:]:
    print(f"{os.path.basename(p)}: {patch(p)}")
