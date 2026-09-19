#!/system/bin/sh
for i in 1 2 3 4; do
  input keyevent KEYCODE_POWER
  sleep 10
  input keyevent KEYCODE_POWER
  sleep 12
done
dmesg | grep -E 'P100|P99|P102|P108'
