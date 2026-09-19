#!/bin/bash
# p79-bootprobe.sh — v27n18 intermittent boot-failure probe (post dtbo fix)
# N cycles: reboot -> wait boot_completed<=150s -> record; on fastboot fallback re-flash and continue
cd "$(dirname "$0")/.." || exit 1
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
LOG=logs/bootprobe-n18.log
N=${1:-5}
echo "=== p79 bootprobe start $(date +%F\ %T) N=$N ===" >> "$LOG"

state() { # echo "<adb-state>|<fb-state>"
  local a="$($ADB devices 2>/dev/null | grep -v '^List' | grep -v '^$' | head -1 | awk '{print $2}')"
  local f="$($FB devices 2>/dev/null | grep -q fastboot && echo fastboot || echo '')"
  echo "$a|$f"
}

for c in $(seq 1 "$N"); do
  echo "--- cycle $c $(date +%T) ---" >> "$LOG"
  st=$(state)
  if [[ "$st" == *"fastboot"* ]]; then
    echo "device in fastboot -> reflash boot_a + set_active a + reboot" >> "$LOG"
    $FB flash boot out/boot-v27n18-q706.img >> "$LOG" 2>&1
    $FB set_active a >> "$LOG" 2>&1
    $FB reboot >> "$LOG" 2>&1
  else
    $ADB reboot >> "$LOG" 2>&1
  fi
  sleep 8
  t0=$(date +%s); result=""
  while :; do
    el=$(( $(date +%s) - t0 ))
    st=$(state)
    adbst="${st%%|*}"; fbst="${st##*|}"
    if [ "$adbst" = "device" ]; then
      bc=$($ADB shell getprop sys.boot_completed 2>/dev/null | tr -d '\r\n')
      if [ "$bc" = "1" ]; then
        up=$($ADB shell uptime 2>/dev/null)
        echo "T+${el}s SUCCESS ($up)" >> "$LOG"
        $ADB shell dmesg > "logs/bootprobe-c${c}-success-dmesg.txt" 2>&1
        result=success; break
      fi
    elif [ -n "$fbst" ]; then
      ra=$($FB getvar slot-retry-count:a 2>&1 | grep -o '[0-9]*$')
      echo "T+${el}s FAIL->FASTBOOT (retry:a=$ra)" >> "$LOG"
      $FB getvar current-slot >> "$LOG" 2>&1
      result=fastboot-fallback; break
    fi
    if [ $el -ge 150 ] && [ -z "$result" ]; then
      if [ "$adbst" = "device" ] || [ "$adbst" = "unauthorized" ] || [ "$adbst" = "offline" ]; then
        echo "T+${el}s HANG>150s (adb=$adbst) -> capture dmesg" >> "$LOG"
        $ADB shell dmesg > "logs/bootprobe-c${c}-hang-dmesg.txt" 2>&1
        $ADB shell "logcat -d -t 500" > "logs/bootprobe-c${c}-hang-logcat.txt" 2>&1
        # keep waiting up to 300s for eventual completion or fallback
        if [ $el -ge 300 ]; then echo "T+${el}s HANG>300s GIVEUP" >> "$LOG"; result=hang; break; fi
      else
        if [ $el -ge 300 ]; then echo "T+${el}s NO-DEVICE>300s GIVEUP" >> "$LOG"; result=gone; break; fi
      fi
    fi
    sleep 5
  done
  echo "cycle $c result: $result" >> "$LOG"
  sleep 10
done
echo "=== p79 bootprobe done $(date +%F\ %T) ===" >> "$LOG"
