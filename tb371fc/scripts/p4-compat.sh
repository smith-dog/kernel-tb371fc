#!/bin/bash
# Cross-reference: TB371FC DTB compatibles vs vanilla kona kernel driver support
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p4-compat.log 2>&1
python3 - <<'PYEOF'
import re, glob

# 1. compatibles required by the device (from clean decompiled DTBs)
compats = set()
for f in sorted(glob.glob("/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs/dtb?.dts")):
    s = open(f, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"compatible\s*=\s*([^;]+);", s):
        for q in re.finditer(r'"([^"]+)"', m.group(1)):
            compats.add(q.group(1))
print(f"DTB compatibles: {len(compats)}")

# 2. compatibles supported by the kernel (drivers + techpack C sources)
kcompats = set()
kpat = re.compile(r'"([a-zA-Z0-9_,.\-]+)"')
files = glob.glob("/home/smith/kernel-van/drivers/**/*.c", recursive=True) + \
        glob.glob("/home/smith/kernel-van/techpack/**/*.c", recursive=True) + \
        glob.glob("/home/smith/kernel-van/sound/**/*.c", recursive=True)
print(f"kernel C files scanned: {len(files)}")
for f in files:
    try:
        s = open(f, encoding="utf-8", errors="replace").read()
    except Exception:
        continue
    for m in re.finditer(r'\.compatible\s*=\s*(.+?),?\s*\n', s):
        for q in re.finditer(r'"([^"]+)"', m.group(1)):
            kcompats.add(q.group(1))
print(f"kernel supported compatibles: {len(kcompats)}")

missing = sorted(c for c in compats if c not in kcompats)
matched = sorted(c for c in compats if c in kcompats)
print(f"DTB-required but kernel-missing: {len(missing)}")
open("/mnt/d/work/code-work/project/tb371fc-kernel/logs/compat-missing.txt", "w").write("\n".join(missing))
open("/mnt/d/work/code-work/project/tb371fc-kernel/logs/compat-matched.txt", "w").write("\n".join(matched))

# 3. show missing, grouped by vendor prefix
import collections
g = collections.defaultdict(list)
for c in missing:
    vendor = c.split(",")[0] if "," in c else "(none)"
    g[vendor].append(c)
for v in sorted(g, key=lambda x: -len(g[x])):
    print(f"--- {v} ({len(g[v])})")
    for c in g[v][:10]:
        print("   ", c)
PYEOF
echo COMPAT_DONE
