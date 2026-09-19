#!/bin/bash
# p95 — reproduce "screen-off then cannot return to desktop" with evidence capture
set -u
LOG=/d/work/code-work/project/tb371fc-kernel/logs/p95-repro-$(date +%H%M%S).txt
adb logcat -c 2>/dev/null
echo "run at $(date)" > "$LOG"
for i in 1 2 3; do
  echo "=== cycle $i ===" >> "$LOG"
  adb shell input keyevent KEYCODE_POWER   # screen off
  sleep 15
  SS_PID_BEFORE=$(adb shell pidof system_server | tr -d '\r')
  adb shell input keyevent KEYCODE_POWER   # screen on
  sleep 6
  SS_PID_AFTER=$(adb shell pidof system_server | tr -d '\r')
  FOCUS=$(adb shell dumpsys window 2>/dev/null | grep mCurrentFocus | tr -d '\r')
  WAKE=$(adb shell dumpsys power 2>/dev/null | grep mWakefulness= | head -1 | tr -d '\r')
  CAP=$(timeout 12 adb shell screencap -p /data/local/tmp/cap.png 2>&1; echo "rc=$?")
  echo "SS before=$SS_PID_BEFORE after=$SS_PID_AFTER" >> "$LOG"
  echo "$FOCUS | $WAKE | $CAP" >> "$LOG"
  HALL=$(adb shell dumpsys sensorservice 2>/dev/null | grep -A3 "Hall Effect Sensor Wakeup: last" | tr -d '\r')
  echo "HALL: $HALL" >> "$LOG"
  sleep 30
done
adb logcat -d > /d/work/code-work/project/tb371fc-kernel/logs/p95-logcat.txt 2>&1
echo done
