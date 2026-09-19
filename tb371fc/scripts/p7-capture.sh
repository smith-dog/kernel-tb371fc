#!/bin/bash
# v7 capture: fastboot -> reader-v7 (copies log, self-navigates to fastboot) -> APatch -> pull
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== waiting for fastboot ==="
OK=0
for i in $(seq 1 60); do
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT; exit 1; }
echo flashing READER-v7
$FB flash boot out/boot-v7-reader.img 2>&1 | tail -1
$FB reboot 2>&1 | tail -1

echo "=== waiting up to 8 min for device to self-enter fastboot (via reader BCB) or user buttons ==="
OK=0
for i in $(seq 1 48); do
  F=$($FB devices 2>/dev/null | grep -c fastboot)
  A=$(adb devices 2>/dev/null | grep -c "device$")
  echo "[+${i}10s] fastboot=$F adb=$A"
  [ "$F" = "1" ] && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_AFTER_READER; exit 1; }
sleep 3
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
mkdir -p out/klogv7
adb shell su -c "ls -la /mnt/vendor/persist/klog-*" 2>&1 | tr -d '\r'
adb pull /mnt/vendor/persist/klog-console.txt out/klogv7/ 2>&1 | tail -1
adb pull /mnt/vendor/persist/klog-pstore-listing.txt out/klogv7/ 2>&1 | tail -1
adb pull /mnt/vendor/persist/klog-readerboot.txt out/klogv7/ 2>&1 | tail -1
echo CAPTUREV7_DONE
