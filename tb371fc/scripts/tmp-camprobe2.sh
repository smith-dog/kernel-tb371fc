#!/system/bin/sh
T=$(ls -t /data/tombstones/tombstone_* | grep -v pb | head -1)
echo "=== $T (head) ==="
head -40 "$T"
echo "=== abort signal/backtrace ==="
grep -A12 "signal" "$T" | head -20
