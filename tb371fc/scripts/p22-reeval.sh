#!/bin/bash
# v12 re-evaluation build: vanilla 157 + display + pstore-ram at CORRECTED
# address 0x27E000000 (the old 0x27E00000 was a TZ carveout -> XPU reset).
# Produces:
#   boot-v12.img     = APatch base + vanilla kernel + fixed DTBs (test boot)
#   boot-harvest.img = APatch base + STOCK kernel + fixed DTBs (harvest boot;
#                      no-map keeps the region intact, KPM reads it later)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p22-reeval.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1

echo "=== 1. kernel config: minidump OFF, pstore ON ==="
./scripts/config -d QCOM_MINIDUMP -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
grep -E "^CONFIG_QCOM_MINIDUMP|^CONFIG_PSTORE_RAM|^CONFIG_PSTORE_CONSOLE" .config

echo "=== 2. strip forensic panic patch from printk.c ==="
grep -q "TB371FC_FORENSIC" kernel/printk/printk.c && {
  sed -i '/TB371FC_FORENSIC/,$d' kernel/printk/printk.c
  echo "forensic patch stripped"
}
grep -c "TB371FC_FORENSIC" kernel/printk/printk.c || echo "printk clean"

echo "=== 3. build Image (incremental) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p22-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p22-make.log | head -5
if [ ! -f arch/arm64/boot/Image ]; then echo BUILD_FAILED; exit 1; fi
cp arch/arm64/boot/Image $OUT/Image-v12
ls -la $OUT/Image-v12

echo "=== 4. DTB surgery: replace poisoned ramoops node with corrected address ==="
python3 - <<'PYEOF'
for n in (0, 1, 2):
    p = f"/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbs/dtb{n}.dts"
    s = open(p, encoding="utf-8").read()
    s = s.replace("ramoops@27e00000 {", "ramoops@27e000000 {")
    s = s.replace("reg = <0x00 0x27e00000 0x00 0x200000>;",
                  "reg = <0x02 0x7e000000 0x00 0x200000>;")
    assert "ramoops@27e000000" in s, f"dtb{n} patch failed"
    assert "0x27e00000 " not in s.replace("0x27e000000 ", ""), f"dtb{n} old address lingers"
    open(p, "w", encoding="utf-8").write(s)
    print(f"dtb{n}.dts -> 0x27E000000 ok")
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

echo "=== 5. assemble both boots ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v12 $OUT/boot-v12.img $OUT/dtbs/tail-new.bin
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Apatch-kernel.bin $OUT/boot-harvest.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v12.img $OUT/boot-harvest.img
echo REEVAL_BUILD_DONE
