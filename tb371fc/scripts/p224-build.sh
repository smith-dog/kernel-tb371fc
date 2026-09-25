#!/bin/bash
# p224 — TASK-034 P3: integrate the 2022-era qcacld trio (era-matched to the
# device FW WLAN.HST.1.0.1.r1-01596 2022-10-07) and build n95v9.
# Patches applied to the 2022 sources: -Werror neutralize, wakeup_source
# registered-lifecycle shims (4.19 lacks CAF init/trash), qdf trace INFO.
exec > /mnt/d/work/code-work/project/devices/tb371fc/logs/p224-build.log 2>&1
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
T34=/home/smith/t34
BASE=/mnt/d/work/code-work/project/devices/tb371fc
SNAP=/home/smith/snapdragon-llvm-10.0.7
STG=$KV/drivers/staging

echo "=== P224 BUILD START $(date) ==="
set -e

echo "--- swap staging trio to 2022 sources ---"
rm -rf $STG/qcacld-3.0 $STG/qca-wifi-host-cmn $STG/fw-api
cp -a $T34/qc22 $STG/qcacld-3.0
cp -a $T34/cmn22 $STG/qca-wifi-host-cmn
cp -a $T34/fw22 $STG/fw-api
rm -rf $STG/qcacld-3.0/.git $STG/qca-wifi-host-cmn/.git $STG/fw-api/.git

echo "--- patch A: -Werror -> -Wno-error (qc22 Kbuild) ---"
sed -i 's/-Werror\b/-Wno-error/' $STG/qcacld-3.0/Kbuild
grep -c 'Wno-error' $STG/qcacld-3.0/Kbuild

echo "--- patch B: wakeup_source shims (cmn22 qdf_lock.c) ---"
python3 - <<'PYEOF'
P = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/staging/qca-wifi-host-cmn/qdf/linux/src/qdf_lock.c"
NL = chr(10)
OLD = ("#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))" + NL +
       "QDF_STATUS qdf_wake_lock_create(qdf_wake_lock_t *lock, const char *name)")
NEW = ("/* p224: CAF wakeup_source_init/trash shims (4.19 mainline PM lacks" + NL +
       " * them); registered lifecycle: add on init, remove on trash */" + NL +
       "static inline void wakeup_source_init(struct wakeup_source *ws, const char *name)" + NL +
       "{" + NL +
       "\tmemset(ws, 0, sizeof(*ws));" + NL +
       "\tws->name = name;" + NL +
       "\twakeup_source_add(ws);" + NL +
       "}" + NL +
       "static inline void wakeup_source_trash(struct wakeup_source *ws)" + NL +
       "{" + NL +
       "\twakeup_source_remove(ws);" + NL +
       "\tmemset(ws, 0, sizeof(*ws));" + NL +
       "}" + NL +
       NL + OLD)
src = open(P).read()
assert src.count(OLD) == 1, "p224 anchor x%d" % src.count(OLD)
open(P, "w").write(src.replace(OLD, NEW, 1))
print("p224: wakeup shims inserted (registered lifecycle)")
PYEOF


echo "--- config sanity ---"
grep -E '^CONFIG_QCA_CLD_WLAN=|^CONFIG_CNSS_QCA6390=' $KV/.config

echo "--- build Image ---"
export PATH=$SNAP/bin:$PATH
cd $KV
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS="-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time -include /home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/staging/fw-api/fw/p226_compat.h" \
     DYNAMIC_SINGLE_CHIP=qca6390 \
     Image
MK=$?
echo "make Image exit=$MK"
if [ "$MK" != "0" ]; then echo BUILD_FAILED; exit 1; fi
cp arch/arm64/boot/Image $BASE/out/Image-v27n95

cd $BASE
python3 tools/repack_boot.py firmware/apatch_patched_11266_0.13.5_clte.img \
    out/Image-v27n95 out/boot-v27n95-pure-q706.img out/dtbs/tail-new.bin 2>&1 | tail -2
python3 tools/repack_boot.py out/kernelsu_patched_20260924_084550.img \
    out/Image-v27n95 out/boot-v27n95-kspatched-flash.img out/dtbs/tail-new.bin 2>&1 | tail -2
echo "=== banner ==="
strings out/Image-v27n95 | grep -m1 "Linux version"
echo "=== md5 ==="
md5sum out/Image-v27n95 out/boot-v27n95-pure-q706.img out/boot-v27n95-kspatched-flash.img
echo "=== P224 BUILD DONE $(date) ==="
