#!/bin/bash
# v6-van forensic capture: fastboot -> reader (log to persist) -> fastboot -> APatch -> pull klog
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== ROUND 1: waiting for fastboot ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_R1; exit 1; }
echo flashing READER image
$FB flash boot out/boot-v6-reader.img 2>&1 | tail -1
$FB reboot 2>&1 | tail -1
echo "=== reader booting: mini-init copies hang log to /persist within ~20s, then hangs at logo ==="
sleep 75

echo "=== ROUND 2: waiting for fastboot (user: power+Vol- when logo frozen) ==="
OK=0
for i in $(seq 1 90); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_R2; exit 1; }
echo restoring APatch backup
$FB flash boot apatch_patched_11266_0.13.5_clte.img 2>&1 | tail -1
$FB set_active a 2>&1 | tail -1
$FB reboot 2>&1 | tail -1

echo "=== waiting for adb ==="
OK=0
for i in $(seq 1 40); do
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_ADB; exit 1; }

echo "=== pulling klog ==="
mkdir -p out/klog
adb shell su -c "ls -la /persist/klog-*" 2>&1 | tr -d '\r'
adb pull /persist/klog-console.txt out/klog/ 2>&1 | tail -1
adb pull /persist/klog-pstore-listing.txt out/klog/ 2>&1 | tail -1
adb pull /persist/klog-readerboot.txt out/klog/ 2>&1 | tail -1
echo CAPTURE_DONE
