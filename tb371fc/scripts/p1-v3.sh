#!/bin/bash
# P1v3: enable pstore ramoops in LOS kernel, inject ramoops node into DTBs,
# build forensics LOS image + reader image (original kernel, patched DTB)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-v3.log 2>&1
echo START
KDIR=/home/smith/android_kernel_xiaomi_sm8250
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
DTSD=$OUT/dtbs

cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:$PATH

echo "=== enable pstore in config ==="
./scripts/config -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "^CONFIG_PSTORE" .config

echo "=== patch DTS files (inject ramoops reserved-memory) ==="
python3 - <<'PYEOF'
NODE = """
		ramoops@27e00000 {
			compatible = "ramoops";
			reg = <0x00 0x27e00000 0x00 0x200000>;
			no-map;
			record-size = <0x20000>;
			console-size = <0x180000>;
			pmsg-size = <0x00>;
			ftrace-size = <0x00>;
		};
"""
for n in (0, 1, 2):
    p = f"/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs/dtb{n}.dts"
    s = open(p, encoding="utf-8").read()
    if "ramoops@27e00000" in s:
        print(f"dtb{n}.dts already patched")
        continue
    marker = "\treserved-memory {\n"
    i = s.find(marker)
    assert i >= 0, f"reserved-memory not found in dtb{n}.dts"
    j = s.find("ranges;", i)
    assert j >= 0, f"ranges not found in dtb{n}.dts"
    j = s.find("\n", j) + 1
    s = s[:j] + NODE + s[j:]
    open(p, "w", encoding="utf-8").write(s)
    print(f"dtb{n}.dts patched")
PYEOF
echo "dtc rc=$?"

echo "=== compile DTBs ==="
cd $DTSD
for n in 0 1 2; do
  dtc -I dts -O dtb -o dtb${n}.new.dtb dtb${n}.dts 2>&1 | grep -vE "Warning" | head -2
done
ls -la dtb*.new.dtb

echo "=== reassemble tail + build images ==="
python3 - <<'PYEOF'
BASE = "/mnt/d/work/code-work/project/tb371fc-kernel"
tail = open(f"{BASE}/out/apatch-dtbtail.bin", "rb").read()
stub = tail[-173:]  # tiny trailing FDT stub kept for fidelity
new_tail = b""
for n in (0, 1, 2):
    new_tail += open(f"{BASE}/out/dtbs/dtb{n}.new.dtb", "rb").read()
new_tail += stub
open(f"{BASE}/out/dtbs/tail-new.bin", "wb").write(new_tail)
print(f"new tail: {len(new_tail)} bytes")

uimg = f"{BASE}/apatch_patched_11266_0.13.5_clte.img"
d = open(uimg, "rb").read()
open(f"{BASE}/out/Apatch-kernel.bin", "wb").write(d[4096:4096+37789136])
print("reader kernel extracted")
PYEOF

python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-p1 $OUT/boot-v3-los.img $DTSD/tail-new.bin
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Apatch-kernel.bin $OUT/boot-v3-reader.img $DTSD/tail-new.bin

ls -la $OUT/
echo V3_DONE
