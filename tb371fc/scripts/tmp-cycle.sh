#!/system/bin/sh
input keyevent KEYCODE_POWER
sleep 15
input keyevent KEYCODE_POWER
sleep 20
dmesg | grep -E "P105|P100|P99|P102|P104"
