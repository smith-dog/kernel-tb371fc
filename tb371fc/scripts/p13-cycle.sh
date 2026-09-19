#!/bin/bash
# p13: full forensic cycle
# 1. flash v11 (hangs at first screen, console fills protected ramoops)
# 2. user buttons -> fastboot
# 3. flash protected APatch boot + deploy dump KPM
# 4. next boot: KPM dumps previous console -> pull
cd /d/work/code-work/project/tb371fc-kernel || exit 1
FB=./platform-tools/fastboot.exe

echo "=== 1. flash v11 hang kernel ==="
adb reboot bootloader 2>&1 | tail -1
sleep 12
for i in 1 2 3 4 5 6; do $FB devices 2>/dev/null | grep -q fastboot && break; sleep 5; done
$FB flash boot out/boot-v11.img 2>&1 | grep -E "OKAY|FAILED"
$FB reboot 2>&1 | tail -1

echo "=== 2. wait 45s (console fills), then waiting for user buttons -> fastboot (8 min) ==="
sleep 45
OK=0
for i in $(seq 1 48); do
  F=$($FB devices 2>/dev/null | grep -c fastboot)
  echo "[wait ${i}] fastboot=$F"
  [ "$F" = "1" ] && { OK=1; break; }
  sleep 10
done
[ "$OK" = "1" ] || { echo NO_FASTBOOT_USER; exit 1; }

echo "=== 3. flash protected APatch boot + deploy dump KPM ==="
$FB flash boot out/boot-apatch-ramoops.img 2>&1 | grep -E "OKAY|FAILED"
$FB set_active a 2>&1 | tail -1
sleep 1
$FB reboot 2>&1 | tail -1

echo "=== 4. wait for boot + deploy KPM ==="
OK=0
for i in $(seq 1 30); do
  sleep 10
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
done
[ "$OK" = "1" ] || { echo NO_ADB_AFTER_PROTECTED_BOOT; exit 1; }
echo "protected boot up; deploying KPM"
adb shell "su -c 'mkdir -p /data/adb/ap/kpm/tb371fc-dump'" 2>&1 | tr -d '\r' | head -1
MSYS_NO_PATHCONV=1 adb push kpms/tb371fc-dump/tb371fc-dump.kpm /data/local/tmp/tb371fc-dump.kpm 2>&1 | tail -1
adb shell "su -c 'cp /data/local/tmp/tb371fc-dump.kpm /data/adb/ap/kpm/tb371fc-dump/tb371fc-dump.kpm && ls -la /data/adb/ap/kpm/tb371fc-dump/'" 2>&1 | tr -d '\r' | head -3

echo "=== 5. reboot: KPM dumps previous console at post-fs-data ==="
adb reboot 2>&1 | tail -1
sleep 60
OK=0
for i in $(seq 1 24); do
  sleep 10
  adb devices 2>/dev/null | grep -q "device$" && { OK=1; break; }
done
[ "$OK" = "1" ] || { echo NO_ADB_AFTER_DUMP_BOOT; exit 1; }
sleep 15
echo "=== 6. pull the dump ==="
adb shell "su -c 'ls -la /data/local/tmp/ramoops-prev.bin'" 2>&1 | tr -d '\r' | head -2
adb shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
MSYS_NO_PATHCONV=1 adb pull /data/local/tmp/ramoops-prev.bin out/ramoops-prev.bin 2>&1 | tail -1
ls -la out/ramoops-prev.bin 2>/dev/null
echo CYCLE_DONE
