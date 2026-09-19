#!/system/bin/sh
input keyevent KEYCODE_POWER
sleep 20
input keyevent KEYCODE_POWER
sleep 15
dmesg | grep -E 'P100|P105|P108'
dmesg | grep -icE 'command transfer failed|underflow'
