#!/bin/bash
# p91 — capture SF/composer kernel stacks during the wake hang
cd "$(dirname "$0")/.." || exit 1
ADB=./platform-tools/adb.exe
LOG=logs/p91-wake-hang-capture.txt
echo "=== p91 start $(date +%T) ===" > "$LOG"

for i in $(seq 1 60); do
  r=$($ADB shell "su -c 'service check input'" 2>/dev/null)
  echo "$r" | grep -q found && { echo "input ready after $((i*5))s" >> "$LOG"; break; }
  sleep 5
done

SFPID=$($ADB shell "ps -A | grep ' surfaceflinger$' | awk '{print \$2}'" | tr -d '\r')
CPPID=$($ADB shell "ps -A | grep composer-service | awk '{print \$2}'" | tr -d '\r')
echo "SF pid=$SFPID composer pid=$CPPID" >> "$LOG"

$ADB shell input keyevent 26
sleep 3
echo "--- screen off done $(date +%T)" >> "$LOG"

$ADB shell input keyevent 224
echo "--- wake sent $(date +%T), capturing stacks rapidly:" >> "$LOG"

for i in $(seq 1 20); do
  {
    echo "=== capture $i ($(date +%T))"
    $ADB shell "su -c 'cat /proc/$SFPID/task/$SFPID/stack 2>/dev/null | head -8'"
    echo "--- composer main:"
    $ADB shell "su -c 'cat /proc/$CPPID/task/$CPPID/stack 2>/dev/null | head -8'"
    echo "--- wchan:"
    $ADB shell "su -c 'cat /proc/$SFPID/task/$SFPID/wchan 2>/dev/null; echo; cat /proc/$CPPID/task/$CPPID/wchan 2>/dev/null; echo'"
  } >> "$LOG" 2>&1
  sleep 2
done

echo "--- final dmesg (V27TRACE + dsi):" >> "$LOG"
$ADB shell "su -c 'dmesg | grep -E \"V27TRACE|dsi_panel|dsi_display\" | tail -40'" >> "$LOG" 2>&1
echo "=== p91 done $(date +%T) ===" >> "$LOG"
