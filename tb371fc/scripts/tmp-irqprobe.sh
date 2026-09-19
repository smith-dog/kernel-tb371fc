#!/system/bin/sh
echo "=== t0 ==="
grep -E '^ *52[89]' /proc/interrupts
echo "=== backlight ramp (DCS traffic) ==="
settings put system screen_brightness 40
sleep 1
settings put system screen_brightness 200
sleep 2
echo "=== t1 ==="
grep -E '^ *52[89]' /proc/interrupts
echo "=== dmesg irq errors ==="
dmesg | grep -icE 'dma_tx done but irq|irq.*not triggered'
