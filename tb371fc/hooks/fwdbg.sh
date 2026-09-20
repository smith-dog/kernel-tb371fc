#!/system/bin/sh
# fwdbg.sh — p121: 开机固件 sysfs 自动喂入 + 取证（正式版）
# 根因：tfa98xx_QS.cnt 的固件 uevent 在声卡注册风暴中丢失（ueventd 未处理，
# pending 节点无人理，60s 超时后 callback(NULL)），直读路径在 Android 无效。
# 本脚本轮询 /sys/class/firmware/，发现 pending 请求即用 /vendor/firmware/
# （含 '!'→'/' 子目录映射）喂入，抢在驱动超时前完成加载。
LOG=/data/local/tmp/fwdbg.log
DUR=240

{
echo "=== fwdbg start $(date) uptime=$(cut -d' ' -f1 /proc/uptime) ==="
} > "$LOG"

# dmesg follower（后台，独占一个子进程；过滤已知刷屏）
(
timeout "$DUR" dmesg -w 2>&1 | grep -avE "Container file not loaded|can't open codec|BE open failed|failed to start some BEs" >> "$LOG"
) &

# 固件 pending 轮询 + 自动喂
(
seen_fed=""
n=0
while [ "$n" -lt $((DUR * 3)) ]; do
  n=$((n + 1))
  pend=$(ls /sys/class/firmware/ 2>/dev/null | grep -v '^timeout$')
  if [ -n "$pend" ]; then
    for f in $pend; do
      [ -d "/sys/class/firmware/$f" ] || continue
      case "$seen_fed" in
        *"|$f|"*) continue ;;
      esac
      echo "uptime=$(cut -d' ' -f1 /proc/uptime) FWPENDING: $f" >> "$LOG"
      src="/vendor/firmware/$f"
      [ -f "$src" ] || src="${f//!//}"      # '!' 是内核对 '/' 的 sysfs 转义
      [ -f "$src" ] || src="/vendor/firmware_mnt/image/$f"
      if [ -f "$src" ]; then
        echo "uptime=$(cut -d' ' -f1 /proc/uptime) MANUAL-FEED: $f ($(stat -c%s "$src" 2>/dev/null) bytes)" >> "$LOG"
        echo 1 > "/sys/class/firmware/$f/loading" 2>/dev/null
        cat "$src" > "/sys/class/firmware/$f/data" 2>/dev/null
        echo 0 > "/sys/class/firmware/$f/loading" 2>/dev/null
        echo "uptime=$(cut -d' ' -f1 /proc/uptime) FED-RC=$? : $f" >> "$LOG"
      else
        echo "uptime=$(cut -d' ' -f1 /proc/uptime) NO-SRC-FILE: $f" >> "$LOG"
      fi
      seen_fed="$seen_fed|$f|"
    done
  fi
  sleep 0.3
done
echo "=== fwdbg poller done $(date) ===" >> "$LOG"
) &
