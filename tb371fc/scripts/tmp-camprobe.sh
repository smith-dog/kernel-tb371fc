#!/system/bin/sh
echo "=== latest tombstone ==="
ls -t /data/tombstones/ | head -3
echo "=== crash logcat ==="
logcat -d -b crash | grep -E "FATAL|Abort message|>>> |pid.*camera" | tail -25
