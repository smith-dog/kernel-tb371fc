#!/system/bin/sh
dmesg | grep -E "BLFIX|P10[0-9]|backlight" | tail -20
echo ---
cat /proc/interrupts | grep -E "dsi|mdss|sde" | head -8
