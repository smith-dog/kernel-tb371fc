#!/bin/bash
# v13: fix the ramoops reservation properly.
# - DTB: drop "compatible" from the ramoops node -> generic no-map reservation
#   (4.19 reserved-memory core skips compatible-nodes; the old node reserved NOTHING)
# - cmdline: ramoops.mem_address=... -> dummy platform device registers ramoops
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p23-v13.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out

echo "=== 1. DTB: strip compatible from ramoops node ==="
python3 - <<'PYEOF'
for n in (0, 1, 2):
    p = f"/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs/dtb{n}.dts"
    s = open(p, encoding="utf-8").read()
    before = s
    s = s.replace('\t\t\tcompatible = "ramoops";\n', '')
    assert s != before, f"dtb{n}: compatible line not found"
    assert "ramoops@27e000000" in s
    open(p, "w", encoding="utf-8").write(s)
    print(f"dtb{n}.dts compatible removed")
PYEOF

cd $OUT/dtbs || exit 1
for n in 0 1 2; do
  dtc -I dts -O dtb -o dtb${n}.new.dtb dtb${n}.dts 2>&1 | grep -viE "warning" | head -2
done
ls -la dtb*.new.dtb

python3 - <<'PYEOF'
BASE = "/mnt/d/work/code-work/project/tb371fc-kernel"
tail = open(f"{BASE}/out/apatch-dtbtail.bin", "rb").read()
stub = tail[-173:]
new_tail = b""
for n in (0, 1, 2):
    new_tail += open(f"{BASE}/out/dtbs/dtb{n}.new.dtb", "rb").read()
new_tail += stub
open(f"{BASE}/out/dtbs/tail-new.bin", "wb").write(new_tail)
print(f"tail-new.bin: {len(new_tail)} bytes")
PYEOF

echo "=== 2. repack v13 + harvest-v13 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v12 $OUT/boot-v13-nocmd.img $OUT/dtbs/tail-new.bin
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Apatch-kernel.bin $OUT/boot-harvest13.img $OUT/dtbs/tail-new.bin

echo "=== 3. cmdline patch on boot-v13-nocmd.img ==="
python3 - <<'PYEOF'
p = "/mnt/d/work/code-work/project/tb371fc-kernel/out/boot-v13-nocmd.img"
d = bytearray(open(p, "rb").read())
cur = d[0x40:0x40+512].split(b"\x00")[0].decode()
print("old cmdline len:", len(cur))
add = " ramoops.mem_address=0x27E000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000"
new = cur + add
assert len(new) <= 510, f"cmdline overflow: {len(new)}"
d[0x40:0x40+512] = new.encode() + b"\x00" * (512 - len(new))
open(p, "wb").write(d)
print("new cmdline len:", len(new))
print("tail of cmdline:", new[-130:])
PYEOF

mv $OUT/boot-v13-nocmd.img $OUT/boot-v13.img
ls -la $OUT/boot-v13.img $OUT/boot-harvest13.img
echo V13_DONE
