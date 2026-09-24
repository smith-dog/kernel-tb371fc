#!/usr/bin/env python3
# p181 (TASK-022 closure): finalize repo completeness.
# 1) remove build junk in tree root (nt36xxx.bc = LLVM bitcode leftover,
#    stdmosyD = temp file);
# 2) commit the p133~p177 patch scripts (README links them - currently 404);
# 3) snapshot the shipped kernel config as tb371fc/config-n86.txt and
#    reference it in the README build section (reproducibility).
import os, shutil, subprocess

KV = "/home/smith/android_kernel_lenovo_paladin"
os.chdir(KV)

# junk
for junk in ("nt36xxx.bc", "stdmosyD"):
    if os.path.exists(junk):
        os.remove(junk)
        print("removed junk:", junk)

# config snapshot
shutil.copy2(".config", "tb371fc/config-n86.txt")
print("config snapshot -> tb371fc/config-n86.txt")

# README build section: config line before the make block
P = "README.md"
c = open(P, encoding="utf-8").read()
old = """```bash
export PATH=/path/to/snapdragon-llvm-10.0.7/bin:$PATH
# 内核镜像"""
new = """内核配置：`cp tb371fc/config-n86.txt .config`（出货 v1.3 内核同款）。

```bash
export PATH=/path/to/snapdragon-llvm-10.0.7/bin:$PATH
# 内核镜像"""
assert c.count(old) == 1, "readme build anchor: %d" % c.count(old)
c = c.replace(old, new)
open(P, "w", encoding="utf-8").write(c)
print("README_OK")
