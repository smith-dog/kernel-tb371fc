#!/bin/bash
# p49 — v27 点火终判（TPIDR_EL0=0 假设的唯一裁决实验）
# 流程: 刷 boot-v27 → 观测 180s → 成功取证（uname/ns/日志）→ 刷回 harvest13
#       失败自动恢复 harvest13 → 等 adb → 收割 ramoops-prev.bin
# 前提: p48 构建成功且金丝雀验证 tpidr_read=False; 设备 adb 在线（harvest13 态）
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27-ignition-timeline.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P49 v27 ignition start"

# 0. 前置检查
[ -f out/boot-v27-q706.img ] || { ts "FATAL: boot-v27-q706.img missing"; exit 1; }
grep -q "tpidr_read=False" logs/p48-v27-gnutriple.log || { ts "WARN: canary verification not confirmed in p48 log"; grep -A2 "tpidr_read" logs/p48-v27-gnutriple.log | tee -a $LOG; }

$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL: device not in adb mode"; exit 1; }
ts "device online (harvest13): $($ADB shell uname -r 2>/dev/null | tr -d '\r')"

# 1. 进 bootloader
ts "rebooting to bootloader"
$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB devices | tee -a $LOG
$FB devices 2>/dev/null | grep -q fastboot || { ts "FATAL: no fastboot"; exit 1; }

# 2. 刷 v27（当前槽 a）
ts "flashing boot-v27-q706.img (slot a)"
$FB flash boot out/boot-v27-q706.img 2>&1 | tee -a $LOG
ts "rebooting (v27)"
$FB reboot 2>&1 | tee -a $LOG
T0=$(date +%s)

# 3. 观测: 3s 精细轮询（review.md 判读框架: authorized=完整开机 / unauthorized=到用户空间后死 / 空=早死）
RESULT=UNKNOWN
LAST_STATE=""
BLIP_SEEN=0
T0=$(date +%s)
while [ $(( $(date +%s) - T0 )) -lt 240 ]; do
  sleep 3
  LINE=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1)
  ST=$(echo "$LINE" | awk '{print $2}')
  [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  if [ "$ST" != "$LAST_STATE" ]; then
    ts "T+$(( $(date +%s) - T0 ))s | adb[$ST] | fb[$( [ $FBL -gt 0 ] && echo yes || echo '')]"
    LAST_STATE=$ST
  fi
  case "$ST" in
    device)     RESULT=BOOTED_AUTHORIZED; break ;;
    unauthorized) BLIP_SEEN=1 ;;
    recovery)   RESULT=BOOTED_RECOVERY; break ;;
  esac
  if [ $FBL -gt 0 ] && [ $BLIP_SEEN -eq 1 ]; then RESULT=FALLBACK_AFTER_USERSPACE; break; fi
  if [ $FBL -gt 0 ] && [ $BLIP_SEEN -eq 0 ] && [ $(( $(date +%s) - T0 )) -gt 150 ]; then RESULT=FASTBOOT_FALLBACK; break; fi
done
ts "observation result: $RESULT blip=$BLIP_SEEN (elapsed $(( $(date +%s) - T0 ))s)"

if [ "$RESULT" = "BOOTED_AUTHORIZED" ] || [ "$RESULT" = "BOOTED_RECOVERY" ]; then
  sleep 15
  ts "=== SUCCESS EVIDENCE ==="
  ts "uname: $($ADB shell uname -a 2>/dev/null | tr -d '\r')"
  ts "ns dirs: $($ADB shell ls /proc/self/ns/ 2>/dev/null | tr '\r\n' ' ')"
  $ADB shell 'zcat /proc/config.gz 2>/dev/null | grep -E "PID_NS=|IPC_NS=|SYSVIPC=|USER_NS="' 2>/dev/null | tr -d '\r' | tee -a $LOG
  $ADB shell 'su -c "ls /sys/fs/pstore/ 2>/dev/null"' 2>/dev/null | tr -d '\r' | tee -a $LOG
  ts "keeping v27 booted briefly for inspection"
  sleep 45
fi

# 4. 恢复 harvest13（成功/失败都恢复，保持日常态）
ts "restoring harvest13"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tee -a $LOG
  $FB set_active a 2>&1 | tee -a $LOG
  $FB reboot 2>&1 | tee -a $LOG
else
  ts "WARN: device not in fastboot for restore; manual restore needed"
fi

# 5. 等 adb 回归 + 收割（harvest13 的 KPM 在 post-fs-data 已自动落盘上一轮 0x27E000000）
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
  ts "adb back: $($ADB shell uname -r 2>/dev/null | tr -d '\r')"
  sleep 15
  MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
  MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/ramoops-v27-harvest.bin 2>&1 | tail -1 | tee -a $LOG
  ts "harvest done: $(md5sum out/ramoops-v27-harvest.bin 2>/dev/null | cut -c1-8)"
  # 解析收割区: ramoops 记录(pstore) / dmesg 文本 / 黑匣子魔数
  python - <<'PYEOF' 2>&1 | tee -a $LOG
import re
try:
    d = open('out/ramoops-v27-harvest.bin','rb').read()
except FileNotFoundError:
    print('no harvest file'); raise SystemExit
print(f'harvest size {len(d)}')
# 黑匣子魔数区
print('bb magic @0x30:', d[0x30:0x40].hex())
print('bb marks @0x40..0x80:', d[0x40:0x80].hex())
# pstore ramoops: console-ramoops 文本特征
txt = d.decode('utf-8', errors='replace')
for pat in ('Booting Linux on physical CPU', 'Kernel panic', 'reboot: ', 'watchdog', 'oops', 'BUG:', 'internal error'):
    hits = [m.start() for m in re.finditer(re.escape(pat), txt)]
    if hits:
        print(f'pattern {pat!r}: {len(hits)} hits at {[hex(h) for h in hits[:4]]}')
        s = max(0, hits[0]-100)
        print('  ctx:', repr(txt[s:hits[0]+300]))
PYEOF
else
  ts "WARN: adb not back after restore"
fi

ts "P49 DONE result=$RESULT"
echo "$RESULT" > logs/v27-ignition-result.txt
