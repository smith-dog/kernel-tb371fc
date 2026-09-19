#!/bin/bash
# p51 — v27c 点火: v27 内核 + insecure ramdisk (ro.secure=0/ro.adb.secure=0)
# 目的: 死亡窗口获得 authorized adb shell → dmesg/logcat = 死因直接证据
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27c-ignition-timeline.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P52 v27c ignition start"
[ -f out/boot-v27c-q706.img ] || { ts FATAL_NO_IMAGE; exit 1; }
$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL: device not in adb"; exit 1; }
ts "device online: $($ADB shell uname -r | tr -d '\r')"

$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB devices 2>/dev/null | grep -q fastboot || { ts "FATAL no fastboot"; exit 1; }
$FB flash boot out/boot-v27c-q706.img 2>&1 | tail -1 | tee -a $LOG
$FB reboot 2>&1 | tail -1 | tee -a $LOG
T0=$(date +%s)

grab() {  # 一次快照, 追加到各文件
  local n; n=$(( $(date +%s) - T0 ))
  {
    echo "===== grab @T+${n}s ====="
    $ADB shell "cat /proc/cmdline; getprop ro.secure; getprop ro.adb.secure; uname -a; ls /dev/watchdog* 2>&1" 2>/dev/null
    echo "----- dmesg tail -----"
    $ADB shell "dmesg | tail -120" 2>/dev/null
    echo "----- logcat events/main tail -----"
    $ADB shell "logcat -d -b all 2>/dev/null | tail -150" 2>/dev/null
  } >> logs/v27c-evidence.txt 2>&1
}

RESULT=UNKNOWN; LAST_AD="(init)"; LAST_FB=0; NGRAB=0
: > logs/v27c-evidence.txt
while [ $(( $(date +%s) - T0 )) -lt 300 ]; do
  sleep 2
  LINE=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1)
  ST=$(echo "$LINE" | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  NOW=$(( $(date +%s) - T0 ))
  if [ "$ST" != "$LAST_AD" ] || { [ $FBL -gt 0 ] && [ $LAST_FB -eq 0 ]; }; then
    ts "T+${NOW}s | adb[$ST] | fb[$( [ $FBL -gt 0 ] && echo yes || echo '')]"
  fi
  LAST_AD=$ST; LAST_FB=$FBL
  if [ "$ST" = "device" ]; then
    RESULT=BOOTED_AUTHORIZED
    grab; NGRAB=$((NGRAB+1))
    # 存活则持续抢收（每 6s 一次，直到死亡/超时）
    if [ $NOW -lt 290 ]; then continue; else break; fi
  fi
  if [ "$ST" = "unauthorized" ]; then RESULT=UNAUTH_STILL; ts "insecure ramdisk not honored"; break; fi
  if [ $FBL -gt 0 ] && [ $NOW -gt 70 ]; then RESULT=FASTBOOT_FALLBACK; break; fi
done
ts "result: $RESULT grabs=$NGRAB (elapsed $(( $(date +%s) - T0 ))s)"

# 恢复 + 收割
ts "restoring harvest13"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 40); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 | tee -a $LOG
else
  ts "WARN: no fastboot — device may still run v27c"
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
[ "$($ADB get-state 2>/dev/null)" = "device" ] && ts "adb back: $($ADB shell uname -r | tr -d '\r')"
ts "P52 DONE result=$RESULT"
echo "$RESULT" > logs/v27c-ignition-result.txt
