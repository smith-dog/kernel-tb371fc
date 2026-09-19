#!/bin/bash
# Capture round for lineage-20 reader: fastboot -> reader20 -> fastboot -> APatch -> pull klog
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== ROUND 1: waiting for fastboot ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_R1; exit 1; }
echo flashing READER-v20
$FB flash boot out/boot-v6-reader20.img 2>&1 | tail -1
$FB reboot 2>&1 | tail -1
echo "=== reader booting: mini-init copies previous crash log to /persist, then loops at logo ==="
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
mkdir -p out/klog20
adb shell su -c "ls -la /persist/klog-*" 2>&1 | tr -d '\r'
adb pull /persist/klog-console.txt out/klog20/ 2>&1 | tail -1
adb pull /persist/klog-pstore-listing.txt out/klog20/ 2>&1 | tail -1
adb pull /persist/klog-readerboot.txt out/klog20/ 2>&1 | tail -1
echo CAPTURE20_DONE
