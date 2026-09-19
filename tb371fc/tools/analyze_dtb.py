#!/usr/bin/env python3
"""Locate and carve DTBs from Android boot/dtbo images, summarize device-tree strings.

Usage: python analyze_dtb.py <image> [<image> ...]
Writes carved DTBs next to the image as <image>.dtbN and prints a string summary.
"""
import re
import struct
import sys
from pathlib import Path

FDT_MAGIC = b"\xd0\x0d\xfe\xed"
DTBO_MAGIC = 0xD7B7AB1E
INTERESTING = [
    (rb"qcom,mdss-dsi-panel-name=\x22([^\x22]+)", "panel-name"),
    (rb"(?:focaltech|goodix|synaptics|elan|himax|ilitek|nt36|gtx|gt9|tp_|touchscreen)[\x20-\x7e]{0,60}", "touch"),
    (rb"[\x20-\x7e]{0,30}(?:smb5|qpnp-smb|qg[-_a-z]*|battery)[\x20-\x7e]{0,30}", "charger/battery"),
    (rb"[\x20-\x7e]{0,20}(?:wcd938x|wcd937x|sm8250|kona|qcom,va|wsa881)[\x20-\x7e]{0,40}", "audio/soc"),
    (rb"[\x20-\x7e]{0,40}lenovo[\x20-\x7e]{0,40}", "lenovo"),
    (rb"qcom,(?:kona|pm8150[a-z]?|usbc|rpmh)[\x20-\x7e]{0,50}", "soc/pmic"),
    (rb"(?:model|compatible)\x00", "node-props"),
]


def carve_dtbs(data: bytes, label: str, outdir: Path):
    found = []
    pos = 0
    while True:
        i = data.find(FDT_MAGIC, pos)
        if i < 0:
            break
        pos = i + 1
        if i + 20 > len(data):
            continue
        total = struct.unpack(">I", data[i + 4 : i + 8])[0]
        off_rsv, off_struct, off_strings = struct.unpack(">III", data[i + 8 : i + 20])
        if not (200 <= total <= len(data) - i):
            continue
        if not (off_rsv < off_struct < off_strings <= total):
            continue
        blob = data[i : i + total]
        found.append((i, blob))
    out = []
    for n, (off, blob) in enumerate(found):
        p = outdir / f"{label}.dtb{n}.dtb"
        p.write_bytes(blob)
        out.append((off, len(blob), str(p)))
    return out


def strings_summary(path: Path):
    data = path.read_bytes()
    strings = re.findall(rb"[\x20-\x7e]{8,}", data)
    joined = b"\n".join(strings)
    print(f"\n=== {path.name} ({len(data)} bytes) ===")
    for pat, tag in INTERESTING:
        hits = sorted({m.group(0).decode("ascii", "replace") for m in re.finditer(pat, joined)})
        if hits:
            show = hits[:12]
            print(f"[{tag}] {len(hits)} unique, sample:")
            for h in show:
                print(f"   {h[:100]}")


def main():
    outdir = Path(sys.argv[0]).parent
    for arg in sys.argv[1:]:
        img = Path(arg)
        data = img.read_bytes()
        magic = struct.unpack(">I", data[:4])[0]
        print(f"\n## {img.name}: size={len(data)} magic=0x{magic:08x}")
        if magic == DTBO_MAGIC:
            total, hsize, esize, ecount, eoff = struct.unpack(">IIIII", data[4:24])
            print(f"  dtbo table: entries={ecount} entry_size={esize}")
            for n in range(ecount):
                base = eoff + n * esize
                dt_size, dt_offset, eid, rev = struct.unpack(">IIII", data[base : base + 16])
                print(f"  entry{n}: id=0x{eid:08x} rev=0x{rev:08x} size={dt_size}")
        dtbs = carve_dtbs(data, img.stem, outdir)
        print(f"  carved {len(dtbs)} DTB(s):")
        for off, size, p in dtbs:
            print(f"    offset={off} size={size} -> {p}")
        for _, _, p in dtbs:
            strings_summary(Path(p))


if __name__ == "__main__":
    main()
