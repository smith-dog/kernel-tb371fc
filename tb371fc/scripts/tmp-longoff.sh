#!/system/bin/sh
input keyevent KEYCODE_POWER
sleep 60
input keyevent KEYCODE_POWER
sleep 15
dmesg | grep -E 'P100|P99|P108' | tail -8
grep msm_drm /proc/interrupts
sleep 5
grep msm_drm /proc/interrupts
