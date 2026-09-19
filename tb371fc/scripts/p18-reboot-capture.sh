#!/bin/bash
# Reboot and race to capture early dmesg before kernel ring rolls over.
# Usage: bash p18-reboot-capture.sh   (from project dir; uses adb in PATH)
BASE="D:/work/code-work/project/tb371fc-kernel"
CAPDIR="$BASE/logs/caps"
mkdir -p "$CAPDIR"
rm -f "$CAPDIR"/cap_*.txt

adb reboot
echo "$(date +%T) reboot issued, waiting for device"
adb wait-for-device
echo "$(date +%T) device online, starting capture race"

for i in $(seq 1 40); do
  ts=$(date +%T)
  adb shell "su -c dmesg" > "$CAPDIR/cap_$i.txt" 2>/dev/null
  sz=$(stat -c %s "$CAPDIR/cap_$i.txt" 2>/dev/null || echo 0)
  echo "$ts cap_$i size=$sz"
  if [ "$sz" -gt 200000 ]; then
    echo "$(date +%T) full ring captured at iteration $i"
    break
  fi
  sleep 1
done
echo CAPTURE_DONE
ls -la "$CAPDIR"
