#!/bin/bash
# Forensic capture: wait fastboot -> flash reader -> wait fastboot -> flash APatch -> pull klog
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== ROUND 1: waiting for fastboot (user: power 15s, then hold Vol-) ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_ROUND1; exit 1; }
echo fastboot detected, flashing READER image
$FB flash boot out/boot-v4-reader.img 2>&1 | tail -1
$FB reboot 2>&1 | tail -1
echo "=== reader booting; it will hang at logo again AFTER copying log; when stuck, press power+Vol- again ==="

echo "=== ROUND 2: waiting for fastboot (user: same button combo when logo freezes) ==="
OK=0
for i in $(seq 1 90); do
  A=$(adb devices 2>/dev/null | grep -c "device$")
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_ROUND2; exit 1; }
echo fastboot detected, restoring APatch image
$FB flash boot apatch_patched_11266_0.13.5_clte.img 2>&1 | tail -1
./platform-tools/fastboot.exe set_active a 2>&1 | tail -1
$FB reboot 2>&1 | tail -1

echo "=== waiting for adb ==="
OK=0
for i in $(seq 1 40); do
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_ADB; exit 1; }

echo "=== pulling klog from /persist ==="
mkdir -p out/klog
adb shell su -c "ls -la /persist/klog-*" 2>&1 | tr -d '\r'
adb pull /persist/klog-console.txt out/klog/ 2>&1 | tail -1
adb pull /persist/klog-pstore-listing.txt out/klog/ 2>&1 | tail -1
adb pull /persist/klog-readerboot.txt out/klog/ 2>&1 | tail -1
adb shell su -c "rm -f /persist/klog-*" 2>&1 | tr -d '\r' | head -1
echo CAPTURE_DONE
