#!/bin/bash
# repack-pure-boot.sh - build a PURE (no-KernelSU) flashable boot image:
# release kernel Image + the pure base's own ramdisk, preserving its v2 header
# and DTB tail. Produces the counterpart of the kspatched release image: same
# kernel bytes, no root at all (for APatch-style setups or no-root use).
#
# usage: repack-pure-boot.sh <pure-base.img> <Image> <out.img>
#
# Every check below reads back the OUTPUT file, not the inputs, so a wrong base
# or a stale Image cannot pass silently.
#
# Byte-reproducibility: the ramdisk is round-tripped through cpio exactly the way
# the kspatched release images are, and `cpio -idm` does not restore directory
# mtimes - so consecutive runs differ in the packed ramdisk by a few bytes
# (md5 changes, content does not). The gates below are content gates, not md5
# comparisons against a stored digest.
set -e
[ $# -eq 3 ] || { echo "usage: $0 <pure-base.img> <Image> <out.img>" >&2; exit 2; }
PUREBASE=$1; KER=$2; IMG=$3
KERNEL_TREE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"   # repo root
W=$(mktemp -d /tmp/repack-pure.XXXXXX); trap 'rm -rf "$W"' EXIT

[ -f "$PUREBASE" ] || { echo "FAIL: pure base $PUREBASE missing" >&2; exit 1; }
[ -f "$KER" ] || { echo "FAIL: kernel Image $KER missing" >&2; exit 1; }
echo "pure_base: $(md5sum "$PUREBASE")"
echo "kernel:    $(md5sum "$KER")"

# the base must genuinely be pure, otherwise "pure" is a lie
mkdir -p "$W/base"
python3 -c '
import struct, sys
b = open(sys.argv[1], "rb").read()
assert b[:8] == b"ANDROID!", b[:8]
ks, rs, page = (struct.unpack("<I", b[8:12])[0], struct.unpack("<I", b[16:20])[0],
                struct.unpack("<I", b[36:40])[0])
off = page + ((ks + page - 1) // page * page)
open(sys.argv[2], "wb").write(b[off:off + rs])
print("base ramdisk off=%d size=%d kernel_size_field=%d" % (off, rs, ks))
' "$PUREBASE" "$W/ramdisk.gz"
( cd "$W/base" && gzip -dc ../ramdisk.gz | cpio -idm --quiet )
base_ksu=$(find "$W/base" -name 'kernelsu.ko' | wc -l)
echo "base ramdisk entries=$(find "$W/base" | wc -l)  kernelsu.ko=$base_ksu"
[ "$base_ksu" = "0" ] || { echo "FAIL: base ramdisk carries kernelsu.ko - not a pure base" >&2; exit 1; }

# round-trip the ramdisk unchanged (cpio -> gzip is what repack_boot.py expects)
( cd "$W/base" && find . | cpio -o -H newc --quiet | gzip -9 > "$W/ramdisk-new.gz" )

python3 "$KERNEL_TREE/tb371fc/tools/repack_boot.py" "$PUREBASE" "$KER" "$IMG" "" "$W/ramdisk-new.gz"

echo "--- read back from the OUTPUT image ---"
python3 -c '
import hashlib, struct, sys
d = open(sys.argv[1], "rb").read()
assert d[:8] == b"ANDROID!", d[:8]
ks, rs, page = (struct.unpack("<I", d[8:12])[0], struct.unpack("<I", d[16:20])[0],
                struct.unpack("<I", d[36:40])[0])
off = page + ((ks + page - 1) // page * page)
open(sys.argv[3], "wb").write(d[off:off + rs])
want = hashlib.md5(open(sys.argv[2], "rb").read()).hexdigest()
got = hashlib.md5(d[page:page + ks]).hexdigest()
print("out kernel_size=%d ramdisk_size=%d page=%d" % (ks, rs, page))
print("embedded_kernel_md5=%s release_image_md5=%s" % (got, want))
print("KERNEL_MATCHES_RELEASE_IMAGE", got == want)
sys.exit(0 if got == want else 1)
' "$IMG" "$KER" "$W/out-ramdisk.gz"

mkdir -p "$W/verify"
( cd "$W/verify" && gzip -dc ../out-ramdisk.gz | cpio -idm --quiet )
out_ksu=$(find "$W/verify" -name 'kernelsu.ko' | wc -l)
echo "out ramdisk entries=$(find "$W/verify" | wc -l)  kernelsu.ko=$out_ksu"
[ "$out_ksu" = "0" ] || { echo "FAIL: output ramdisk contains kernelsu.ko" >&2; exit 1; }

echo "banner: $(strings "$IMG" | grep -m1 'Linux version')"
echo "output: $(ls -l "$IMG")"
echo "md5:    $(md5sum "$IMG")"
echo "sha256: $(sha256sum "$IMG")"
echo "PURE_REPACK=SUCCESS $IMG"
