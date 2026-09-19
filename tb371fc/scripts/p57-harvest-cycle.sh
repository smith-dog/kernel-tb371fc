#!/bin/bash
# p57 — 两阶段收割实验
# 阶段A: 验证 harvest14 干净启动 + KPM memremap 在 no-map 区域上安全
# 阶段B: v27 点火(死亡) → harvest14 收割 v27 死亡现场 → 解析 → 恢复 harvest13
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/p57-harvest.log
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

wait_adb() { # $1=max*3s
  for i in $(seq 1 ${1:-60}); do
    sleep 3
    [ "$($ADB devices 2>/dev/null | grep -c "$SERIAL	device")" -ge 1 ] && return 0
  done
  return 1
}
wait_fb() {
  for i in $(seq 1 ${1:-30}); do
    sleep 2
    $FB devices 2>/dev/null | grep -q fastboot && return 0
  done
  return 1
}

ts "=== 阶段A: harvest14 干净启动验证 ==="
[ "$($ADB get-state 2>/dev/null)" = "device" ] || { ts "FATAL: no adb"; exit 1; }
$ADB reboot bootloader
wait_fb 30 || { ts "FATAL no fastboot"; exit 1; }
$FB flash boot out/boot-harvest14.img 2>&1 | tail -1 | tee -a $LOG
$FB set_active a 2>&1 | tail -1 >/dev/null
$FB reboot >/dev/null 2>&1
if wait_adb 80; then
  ts "harvest14 adb UP: $($ADB shell uname -r | tr -d '\r')"
  ts "iomem 27e line: $($ADB shell 'su -c "grep -i 27e /proc/iomem"' 2>/dev/null | tr -d '\r')"
  sleep 20
  $ADB shell "su -c 'ls -la /data/local/tmp/ramoops-prev.bin'" 2>&1 | tr -d '\r' | tee -a $LOG
  MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
  MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/p57-baseline.bin >/dev/null 2>&1
  ts "baseline pulled: $(md5sum out/p57-baseline.bin 2>/dev/null | cut -c1-8) size=$(wc -c < out/p57-baseline.bin 2>/dev/null)"
  A_OK=1
else
  ts "harvest14 adb TIMEOUT (200s) — mark A_FAIL"
  A_OK=0
fi

ts "=== 阶段B: v27 点火 → 死亡 → harvest14 收割 ==="
if [ $A_OK -eq 1 ]; then
  $ADB reboot bootloader
  wait_fb 30 || { ts "FATAL no fastboot B"; exit 1; }
  $FB flash boot out/boot-v27-q706.img 2>&1 | tail -1 | tee -a $LOG
  $FB reboot >/dev/null 2>&1
  T0=$(date +%s)
  RESULT=UNKNOWN; LAST="(i)"
  while [ $(( $(date +%s) - T0 )) -lt 130 ]; do
    sleep 2
    ST=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1 | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
    FBL=$($FB devices 2>/dev/null | grep -c fastboot)
    [ "$ST" != "$LAST" ] && { ts "v27 T+$(( $(date +%s) - T0 ))s adb[$ST] fb[$( [ $FBL -gt 0 ] && echo y)]"; LAST=$ST; }
    if [ $FBL -gt 0 ]; then RESULT=FB_AT_$(( $(date +%s) - T0 ))s; break; fi
  done
  ts "v27 death cycle: $RESULT"
  # 此时设备在 fastboot → 直接刷 harvest14
  $FB flash boot out/boot-harvest14.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a >/dev/null 2>&1
  $FB reboot >/dev/null 2>&1
  if wait_adb 80; then
    ts "harvest14(recover) UP"
    sleep 25   # KPM post-fs-data 落盘
    MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
    MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/ramoops-v27-death.bin >/dev/null 2>&1
    ts "DEATH DUMP pulled: $(md5sum out/ramoops-v27-death.bin 2>/dev/null | cut -c1-8) size=$(wc -c < out/ramoops-v27-death.bin 2>/dev/null)"
  else
    ts "harvest14(recover) adb TIMEOUT"
  fi
fi

ts "=== 恢复 harvest13 ==="
$ADB reboot bootloader 2>/dev/null
wait_fb 45 || { ts "CRITICAL: no fastboot for final restore"; cd /d/work/code-work && python call_me.py "平板无法自动恢复 请音量减+电源进fastboot" >/dev/null 2>&1; exit 9; }
$FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
$FB set_active a >/dev/null 2>&1
$FB reboot >/dev/null 2>&1
if wait_adb 60; then ts "daily restored: $($ADB shell uname -r | tr -d '\r')"; else ts "final adb timeout (may still boot)"; fi
ts "P57 DONE A_OK=$A_OK"
