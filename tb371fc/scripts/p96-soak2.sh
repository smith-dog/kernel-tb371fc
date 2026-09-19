#!/bin/bash
# extended soak: 20 cycles, varied off-durations incl. long doze
L() { adb shell su -c "$1" 2>/dev/null | tr -d '\r'; }
echo "=== SOAK2 START $(date +%T) ==="
fail=0
for i in $(seq 1 20); do
  off=$(( (i % 4) * 10 + 3 ))   # 3,13,23,33s pattern
  adb shell input keyevent KEYCODE_POWER; sleep $off
  adb shell input keyevent KEYCODE_POWER; sleep 7
  mst=$(adb shell dumpsys display 2>/dev/null | grep -m1 'mState=' | tr -d '\r ' | cut -d= -f2)
  bl=$(L "cat /sys/class/backlight/ktz8866-a/backlight")
  if timeout 12 adb exec-out screencap -p > /dev/null 2>&1; then cap=OK; else cap=HANG; fi
  echo "round $i (off=${off}s): mState=$mst bl=$bl cap=$cap"
  { [ "$cap" = "HANG" ] || [ "$mst" != "ON" ]; } && { fail=1; echo "FAIL round $i"; break; }
done
echo "--- errors after soak2 ---"
L "dmesg | grep -c 'Command transfer failed'"
L "dmesg | grep -c 'dma_tx done but irq'"
echo "FAIL=$fail  === SOAK2 END $(date +%T) ==="
