#!/bin/bash
# v8 experiment: DFPS-off dtbo + reader-v7 kernel; auto restore; pull klog
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== waiting for fastboot (user buttons) ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT; exit 1; }

echo "=== flash DFPS-off dtbo + reader-v7 kernel ==="
$FB flash dtbo out/dtbo-dfpsoff.img 2>&1 | grep -E "OKAY|FAILED"
$FB flash boot out/boot-v7-reader.img 2>&1 | grep -E "OKAY|FAILED"
$FB reboot 2>&1 | tail -1

echo "=== wait up to 4 min for reader self-nav to fastboot (success signal) ==="
OK=0
for i in $(seq 1 24); do
  sleep 10
  F=$($FB devices 2>/dev/null | grep -c fastboot)
  A=$(adb devices 2>/dev/null | grep -c "device$")
  echo "[+${i}0s] fastboot=$F adb=$A"
  if [ "$F" = "1" ]; then OK=1; echo SELF_NAV_OR_FALLBACK_DETECTED; break; fi
done
if [ "$OK" = "0" ]; then
  echo "NO_SELF_NAV - need user buttons to fastboot; waiting 10 min"
  for i in $(seq 1 60); do
    sleep 10
    $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  done
  [ "$OK" = "1" ] || { echo STILL_NO_FASTBOOT; exit 1; }
fi
sleep 3

echo "=== restore APatch boot (keep modified dtbo) ==="
$FB flash boot apatch_patched_11266_0.13.5_clte.img 2>&1 | grep -E "OKAY|FAILED"
$FB set_active a 2>&1 | tail -1
$FB reboot 2>&1 | tail -1

echo "=== wait adb, pull klog ==="
OK=0
for i in $(seq 1 40); do
  sleep 10
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
done
[ "$OK" = "1" ] || { echo NO_ADB; exit 1; }
mkdir -p out/klogv8
adb shell su -c "ls -la /mnt/vendor/persist/klog-*" 2>&1 | tr -d '\r'
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-console.txt out/klogv8/ 2>&1 | tail -1
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-pstore-listing.txt out/klogv8/ 2>&1 | tail -1
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-readerboot.txt out/klogv8/ 2>&1 | tail -1
echo CAPTUREV8_DONE
