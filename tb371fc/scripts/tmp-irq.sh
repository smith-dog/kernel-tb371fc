#!/system/bin/sh
cat /proc/interrupts | grep -E '^ *52[0-9]'
