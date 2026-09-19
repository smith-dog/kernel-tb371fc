#!/bin/bash
# p96 soak: 12x power off/on cycles, verify wake health each round
L() { adb shell "su -c '$1'" 2>/dev/null | tr -d '\r'; }
echo "=== SOAK START $(date +%T) ==="
for i in $(seq 1 12); do
  adb shell input keyevent KEYCODE_POWER; sleep 6
  adb shell input keyevent KEYCODE_POWER; sleep 6
  state=$(L "cat /sys/class/backlight/ktz8866-a/backlight 2>/dev/null || cat /sys/class/backlight/*/brightness" | head -1)
  st=$(adb shell dumpsys display 2>/dev/null | grep -m1 'mState=' | tr -d '\r')
  err=$(L "dmesg | grep -c 'Command transfer failed'")
  storm=$(L "dmesg | grep -c 'dma_tx done but irq'")
  # screencap with 10s timeout to prove SF is producing frames
  if timeout 12 adb exec-out screencap -p > /dev/null 2>&1; then cap=OK; else cap=HANG; fi
  echo "round $i: bl=$state $st err=$err storm=$storm screencap=$cap"
  [ "$cap" = "HANG" ] && { echo "SCREENCAP HANG at round $i"; break; }
done
echo "--- dmesg tail ---"
L "dmesg | tail -30" | grep -iE 'dsi|dma|panel|backlight' | tail -15
echo "=== SOAK END $(date +%T) ==="
