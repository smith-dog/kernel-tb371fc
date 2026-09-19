#!/bin/bash
# Full automated forensic cycle via adb+fastboot
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== 1. adb reboot to bootloader ==="
adb reboot bootloader 2>&1 | tail -1
OK=0
for i in $(seq 1 20); do
  sleep 5
  $FB devices 2>/dev/null | grep -q fastboot && { OK=1; break; }
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT; exit 1; }
echo "=== 2. flash reader-v7 ==="
$FB flash boot out/boot-v7-reader.img 2>&1 | tail -1
$FB reboot 2>&1 | tail -1
echo "=== 3. wait up to 3 min: reader copies log then self-navigates to fastboot ==="
OK=0
for i in $(seq 1 18); do
  sleep 10
  F=$($FB devices 2>/dev/null | grep -c fastboot)
  A=$(adb devices 2>/dev/null | grep -c "device$")
  echo "[+${i}0s] fastboot=$F adb=$A"
  if [ "$F" = "1" ]; then OK=1; echo READER_NAVIGATED; break; fi
done
if [ "$OK" = "0" ]; then
  echo "NO_SELF_NAVIGATION - device likely hung pre-init"
  echo "PLEASE hold power+Vol- to enter fastboot; waiting 10 min..."
  for i in $(seq 1 60); do
    sleep 10
    F=$($FB devices 2>/dev/null | grep -c fastboot)
    echo "[r2+${i}0s] fastboot=$F"
    [ "$F" = "1" ] && { OK=1; break; }
  done
  [ "$OK" = "1" ] || { echo STILL_NO_FASTBOOT_GIVE_UP; exit 1; }
fi
sleep 3
echo "=== 4. restore APatch backup ==="
$FB flash boot apatch_patched_11266_0.13.5_clte.img 2>&1 | tail -1
$FB set_active a 2>&1 | tail -1
$FB reboot 2>&1 | tail -1
echo "=== 5. wait adb ==="
OK=0
for i in $(seq 1 40); do
  sleep 10
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
done
[ "$OK" = "1" ] || { echo NO_ADB; exit 1; }
echo "=== 6. pull klog ==="
mkdir -p out/klogv7
adb shell su -c "ls -la /mnt/vendor/persist/klog-*" 2>&1 | tr -d '\r'
adb pull /mnt/vendor/persist/klog-console.txt out/klogv7/ 2>&1 | tail -1
adb pull /mnt/vendor/persist/klog-pstore-listing.txt out/klogv7/ 2>&1 | tail -1
adb pull /mnt/vendor/persist/klog-readerboot.txt out/klogv7/ 2>&1 | tail -1
echo CAPTUREV7_FULL_DONE
