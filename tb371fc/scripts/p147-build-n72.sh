#!/bin/bash
# p139 build: v27n72 = p133+p134 gesture + p137(p+header) pstore dcache flush
# + ramoops cmdline. Repacked with the MANAGER-PATCHED ramdisk extracted from
# kernelsu_patched_20260920_114142.img (ksu.ko+ksud injected by the user's
# manager) so the image self-contains root for forensic iteration.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p147-build.log 2>&1
echo "=== P147 BUILD START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
PATCHED=/mnt/d/work/code-work/logs/dt60/kernelsu_patched_20260920_114142.img
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image 2>&1 | tail -8
MK=${PIPESTATUS[0]}
echo "make exit=$MK"
if [ "$MK" != "0" ]; then echo "BUILD FAILED"; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n72
# extract patched ramdisk
python3 - "$PATCHED" "$BASE/out/ramdisk-114142.img" << 'PYEOF'
import struct, sys
data = open(sys.argv[1], "rb").read()
assert data[:8] == b"ANDROID!"
ks = struct.unpack("<I", data[8:12])[0]
rs = struct.unpack("<I", data[16:20])[0]
page = struct.unpack("<I", data[36:40])[0]
def pad(n): return (n + page - 1) // page * page
r_off = page + pad(ks)
rd = data[r_off:r_off+rs]
open(sys.argv[2], "wb").write(rd)
print(f"ramdisk extracted: {rs} bytes at 0x{r_off:x}")
PYEOF
[ -s $BASE/out/ramdisk-114142.img ] || { echo RAMDISK_EXTRACT_FAILED; exit 1; }
cd $BASE
python3 tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27n72 out/boot-v27n72-patched-q706.img out/dtbs/tail-new.bin out/ramdisk-114142.img "ramoops.mem_address=0x27e000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000 ramoops.record_size=0x20000" || { echo REPACK_FAILED; exit 1; }
echo "=== artifacts ==="
ls -la out/Image-v27n72 out/boot-v27n72-patched-q706.img
echo "=== md5 ==="
md5sum out/Image-v27n72 out/boot-v27n72-patched-q706.img
echo "=== P147 BUILD DONE $(date) ==="
