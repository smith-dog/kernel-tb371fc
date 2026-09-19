#!/bin/bash
# p58 — v27g 点火: v27 + ramdisk(/adb_keys 预授权) + tail-new
# 目标: adbd 瞬态窗口内 authorized → 抢收 dmesg/logcat = 死因直接证据
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27g-ignition-timeline.txt
: > $LOG
: > logs/v27g-dmesg.txt
: > logs/v27g-logcat.txt
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P58 v27g ignition"
$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL no adb"; exit 1; }
$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB flash boot out/boot-v27g-q706.img 2>&1 | tail -1 | tee -a $LOG
$FB reboot >/dev/null 2>&1
T0=$(date +%s)
GRAB=0; RESULT=UNKNOWN; LAST="(i)"
while [ $(( $(date +%s) - T0 )) -lt 200 ]; do
  sleep 1
  ST=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1 | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  NOW=$(( $(date +%s) - T0 ))
  if [ "$ST" != "$LAST" ]; then ts "T+${NOW}s adb[$ST] fb[$( [ $FBL -gt 0 ] && echo y)]"; fi
  LAST=$ST
  if [ "$ST" = "device" ]; then
    RESULT=AUTHORIZED
    ts ">>> AUTHORIZED at T+${NOW}s — capturing"
    ($ADB shell "dmesg" > logs/v27g-dmesg.txt 2>/dev/null; echo "DMESG_DONE $(( $(date +%s) - T0 ))s" >> $LOG) &
    ($ADB shell "while true; do logcat -d -b all; sleep 2; done" > logs/v27g-logcat.txt 2>/dev/null) &
    sleep 40   # 尽量多存活取证
    GRAB=1
    break
  fi
  if [ $FBL -gt 0 ] && [ $NOW -gt 70 ]; then RESULT=FB_FALLBACK; break; fi
done
wait 2>/dev/null
ts "result=$RESULT grab=$GRAB dmesg=$(wc -c < logs/v27g-dmesg.txt 2>/dev/null)B logcat=$(wc -c < logs/v27g-logcat.txt 2>/dev/null)B"

ts "restore harvest13"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 45); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a >/dev/null 2>&1
  $FB reboot >/dev/null 2>&1
else
  ts "CRITICAL no fastboot"; cd /d/work/code-work && python call_me.py "平板挂死请音量减+电源进fastboot" >/dev/null 2>&1
  exit 9
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
[ "$($ADB get-state 2>/dev/null)" = "device" ] && ts "daily restored"
ts "P58 DONE $RESULT"
