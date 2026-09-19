#!/bin/bash
# P1v3b: rebuild kernel with pstore, redo DTB patch (correct anchor), build images
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-v3b.log 2>&1
echo START
KDIR=/home/smith/android_kernel_xiaomi_sm8250
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
DTSD=$OUT/dtbs

cd $KDIR || exit 1
export PATH=/usr/lib/llvm-17/bin:$PATH

echo "=== make Image with pstore ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- KCFLAGS=-Wno-error Image > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-make-v3.log 2>&1
MAKE_RC=$?
echo "make exit=$MAKE_RC"
if [ -f arch/arm64/boot/Image ]; then
  cp arch/arm64/boot/Image $OUT/Image-p1v3
  echo IMAGE_OK
else
  tail -5 /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-make-v3.log
  echo BUILD_FAILED
  exit 1
fi

echo "=== decompile all 3 dtbs fresh ==="
cd $DTSD
for n in 0 1 2; do
  dtc -I dtb -O dts -o dtb${n}.dts dtb${n}.dtb 2>/dev/null
  ls dtb${n}.dts
done

echo "=== patch with corrected anchor ==="
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
    marker = "\t\thyp_region@80000000 {"
    i = s.find(marker)
    assert i >= 0, f"hyp_region anchor not found in dtb{n}.dts"
    s = s[:i] + NODE.lstrip("\n") + "\n" + s[i:]
    open(p, "w", encoding="utf-8").write(s)
    print(f"dtb{n}.dts patched")
PYEOF

echo "=== compile patched dtbs ==="
cd $DTSD
for n in 0 1 2; do
  dtc -I dts -O dtb -o dtb${n}.new.dtb dtb${n}.dts 2>&1 | grep -iE "error|fatal" | head -2
  ls -la dtb${n}.new.dtb || exit 1
done

echo "=== reassemble + repack ==="
python3 - <<'PYEOF'
BASE = "/mnt/d/work/code-work/project/tb371fc-kernel"
tail = open(f"{BASE}/out/apatch-dtbtail.bin", "rb").read()
stub = tail[-173:]
new_tail = b""
for n in (0, 1, 2):
    new_tail += open(f"{BASE}/out/dtbs/dtb{n}.new.dtb", "rb").read()
new_tail += stub
open(f"{BASE}/out/dtbs/tail-new.bin", "wb").write(new_tail)
print(f"new tail: {len(new_tail)} bytes")

d = open(f"{BASE}/apatch_patched_11266_0.13.5_clte.img", "rb").read()
open(f"{BASE}/out/Apatch-kernel.bin", "wb").write(d[4096:4096+37789136])
print("reader kernel extracted")
PYEOF

python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-p1v3 $OUT/boot-v3-los.img $DTSD/tail-new.bin
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Apatch-kernel.bin $OUT/boot-v3-reader.img $DTSD/tail-new.bin

ls -la $OUT/
echo V3B_DONE
