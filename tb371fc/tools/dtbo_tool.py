#!/usr/bin/env python3
"""Unpack QC dtbo.img entries and dump DFPS/DSC related properties."""
import re
import struct
import sys
from pathlib import Path

img = Path(sys.argv[1] if len(sys.argv) > 1 else "fw/TB371FC_ZUI_16.0.474/image/dtbo.img")
outdir = Path(sys.argv[2] if len(sys.argv) > 2 else "out/dtbo")
outdir.mkdir(parents=True, exist_ok=True)

d = img.read_bytes()
magic, total, hsize, esize, ecount, eoff, page, ver = struct.unpack(">8I", d[:32])
assert magic == 0xD7B7AB1E, hex(magic)
print(f"entries={ecount} esize={esize} page={page} ver={ver}")

entries = []
for n in range(ecount):
    base = eoff + n * esize
    dt_size, dt_offset, eid, rev = struct.unpack(">IIII", d[base : base + 16])
    entries.append((n, dt_size, dt_offset, eid, rev))

for n, sz, off, eid, rev in entries:
    blob = d[off : off + sz]
    p = outdir / f"ov{n}.dtb"
    p.write_bytes(blob)
    s = blob.decode("utf-8", "replace")
    strings = re.findall(r"[\x20-\x7e]{6,}", s)
    panel = [x for x in strings if "panel-name" in x.lower() or "mdss-dsi-panel" in x.lower()]
    dfps = [x for x in strings if "dfps" in x.lower()]
    dsc = [x for x in strings if "dsc" in x.lower()]
    print(f"ov{n}: size={sz} dfps={len(dfps)} dsc={len(dsc)} panel_props={len(panel)}")
    for x in panel[:2]:
        print("   PANEL:", x[:80])

# quick scan: find all dfps/dsc property NAMES across all overlays
allprops = set()
for n, sz, off, eid, rev in entries:
    blob = d[off : off + sz]
    for m in re.finditer(rb"[\x20-\x7e]{4,}", blob):
        t = m.group(0).decode("ascii", "replace")
        if t.startswith(("qcom,", "lenovo,")) and ("dfps" in t or "dsc" in t or "fps" in t):
            allprops.add(t)
print("=== dfps/dsc related property names ===")
for t in sorted(allprops)[:30]:
    print("  ", t)
