#!/bin/bash
# v12 experiment: reader-v7 + DFPS-off dtbo already flashed; poll fastboot -> reader -> restore -> pull
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== ROUND 1: waiting for fastboot ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_R1; exit 1; }

echo "=== flash READER-v7 (boot only; DFPS-off dtbo stays) ==="
$FB flash boot out/boot-v7-reader.img 2>&1 | grep -E "OKAY|FAILED"
$FB reboot 2>&1 | tail -1

echo "=== wait up to 3 min: reader self-nav to fastboot = mini-init ran (log captured!) ==="
OK=0
for i in $(seq 1 18); do
  sleep 10
  F=$($FB devices 2>/dev/null | grep -c fastboot)
  A=$(adb devices 2>/dev/null | grep -c "device$")
  echo "[+${i}0s] fastboot=$F adb=$A"
  if [ "$F" = "1" ]; then OK=1; echo READER_SELF_NAV_SUCCESS; break; fi
done

echo "=== restore APatch backup ==="
if [ "$OK" = "0" ]; then
  echo "no self-nav; need user buttons again if not already in fastboot"
fi
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_R2; exit 1; }
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
mkdir -p out/klogv12
adb shell su -c "ls -la /mnt/vendor/persist/klog-*" 2>&1 | tr -d '\r'
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-console.txt out/klogv12/ 2>&1 | tail -1
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-pstore-listing.txt out/klogv12/ 2>&1 | tail -1
MSYS_NO_PATHCONV=1 adb pull /mnt/vendor/persist/klog-readerboot.txt out/klogv12/ 2>&1 | tail -1
echo CAPTUREV12_DONE
