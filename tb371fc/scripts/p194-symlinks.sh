#!/bin/bash
# p194-symlinks.sh — retarget all dangling symlinks pointing at the old
# kernel tree (/home/smith/android_kernel_lenovo_paladin) to the new tree,
# then resume `make modules` and collect ksu.ko + dlkm .ko files.
KV=/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc
BASE=/mnt/d/work/code-work/project/devices/tb371fc
export PATH=/home/smith/snapdragon-llvm-10.0.7/bin:$PATH
cd "$KV" || { echo NO_TREE; exit 1; }

echo "=== dangling symlinks before ==="
find . -xtype l 2>/dev/null | tee /tmp/p194-dangling.txt
N=0
while IFS= read -r l; do
    t=$(readlink "$l")
    case "$t" in
        /home/smith/android_kernel_lenovo_paladin/*)
            nt="${t/\/home\/smith\/android_kernel_lenovo_paladin/$KV}"
            if [ -e "$nt" ]; then
                ln -sfn "$nt" "$l" && N=$((N+1))
            else
                echo "NO_TARGET $l -> $nt"
            fi
            ;;
        *)
            echo "SKIP_EXTERNAL $l -> $t"
            ;;
    esac
done < /tmp/p194-dangling.txt
echo "retargeted=$N"
echo "=== dangling after ==="
find . -xtype l 2>/dev/null | head -20
echo "=== make modules ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     modules
MK=$?
echo "make modules exit=$MK"
if [ "$MK" != "0" ]; then echo MODULES_FAILED; grep -nE "error:" /mnt/d/work/code-work/project/devices/tb371fc/logs/p194-modules.log | head; exit 1; fi

mkdir -p $BASE/out/ko-v27n89
find . -name '*.ko' -newer .config -exec cp {} $BASE/out/ko-v27n89/ \; 2>/dev/null
ls $BASE/out/ko-v27n89 | wc -l
ls -la $BASE/out/ko-v27n89/ksu.ko
md5sum $BASE/out/Image-v27n89
echo "=== P194 DONE $(date) ==="
