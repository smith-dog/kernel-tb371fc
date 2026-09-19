#!/system/bin/sh
# capture: watch system_server pid; when it changes, dump newest watchdog dropbox entry
last=$(ps -A | grep system_server | awk '{print $2}')
echo "start ss=$last"
i=0
while [ $i -lt 20 ]; do
  sleep 15
  cur=$(ps -A | grep system_server | awk '{print $2}')
  if [ -n "$cur" ] && [ "$cur" != "$last" ]; then
    echo "RESTART detected: $last -> $cur at $(date +%H:%M:%S)"
    last=$cur
  elif [ -z "$cur" ]; then
    echo "no system_server at $(date +%H:%M:%S)"
  fi
  i=$((i+1))
done
echo "final ss=$last"
dumpsys dropbox 2>/dev/null | grep -E '^2026-09-18 19:4' | tail -10
