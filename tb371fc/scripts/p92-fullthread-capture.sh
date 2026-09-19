#!/bin/bash
# p92 — full-thread kernel stack capture during wake hang
cd "$(dirname "$0")/.." || exit 1
ADB=./platform-tools/adb.exe
LOG=logs/p92-wake-hang-capture.txt
echo "=== p92 start $(date +%T) ===" > "$LOG"

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
echo "--- screen off $(date +%T)" >> "$LOG"
$ADB shell input keyevent 224
echo "--- wake $(date +%T) - capturing ALL composer+SF thread stacks:" >> "$LOG"

for i in $(seq 1 10); do
  {
    echo "=== round $i ($(date +%T))"
    $ADB shell "su -c 'for t in /proc/$CPPID/task/*; do c=\$(cat \$t/comm 2>/dev/null); s=\$(cat \$t/stack 2>/dev/null | head -5 | tr \"\n\" \";\"); w=\$(cat \$t/wchan 2>/dev/null); echo \"[CP \$c wchan=\$w] \$s\"; done'"
    $ADB shell "su -c 'for t in /proc/$SFPID/task/*; do c=\$(cat \$t/comm 2>/dev/null); w=\$(cat \$t/wchan 2>/dev/null); echo \"[SF \$c] \$w\"; done'"
  } >> "$LOG" 2>&1
  sleep 3
done
echo "=== p92 done $(date +%T) ===" >> "$LOG"
