#!/bin/bash
# p55 — 失联设备守望: 等 fastboot 出现 → 自动刷回 harvest13 → 验证 adb
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
LOG=logs/p55-restore-watch.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }
ts "watcher start: waiting for fastboot (up to 55 min)"
for i in $(seq 1 330); do
  sleep 10
  if $FB devices 2>/dev/null | grep -q fastboot; then
    ts "FASTBOOT appeared (iter $i)"
    sleep 3
    $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
    $FB set_active a 2>&1 | tail -1 | tee -a $LOG
    $FB reboot 2>&1 | tail -1 | tee -a $LOG
    break
  fi
  # 设备直接进了 adb(意外)也提示
  if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
    ts "ADB appeared directly (iter $i) — unexpected; NOT flashing (device may be on v27d)"
    break
  fi
done
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
  ts "adb back: $($ADB shell uname -r | tr -d '\r')"
  ts "RESTORE_OK"
else
  ts "RESTORE_INCOMPLETE (no adb)"
fi
