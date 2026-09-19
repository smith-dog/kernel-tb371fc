#!/bin/bash
# p50 — v27a 点火: boot 头 cmdline 追加 androidboot.adb.secure=0
# 目的: 死亡窗口(T+50~60s)内获得 authorized adb → 抢收 logcat/dmesg = 死因直接证据
# 附加: 先试 fastboot boot(临时引导, review 线索7); 不支持再落刷
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27a-ignition-timeline.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P50 v27a ignition start"

# 1. repack v27a = Image-v27 + tail-new + cmdline append
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img \
    out/Image-v27q706 out/boot-v27a-q706.img out/dtbs/tail-new.bin "" \
    "androidboot.adb.secure=0" 2>&1 | tee -a $LOG
[ -f out/boot-v27a-q706.img ] || { ts "FATAL repack"; exit 1; }
md5sum out/boot-v27a-q706.img | tee -a $LOG

$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL: device not in adb"; exit 1; }
ts "device online: $($ADB shell uname -r | tr -d '\r')"

# 2. bootloader
$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB devices 2>/dev/null | grep -q fastboot || { ts "FATAL no fastboot"; exit 1; }

# 3. 优先临时引导
ts "trying fastboot boot (temporary)"
if timeout 20 $FB boot out/boot-v27a-q706.img > logs/v27a-fastboot-boot.txt 2>&1 && \
   ! grep -qiE "unknown command|FAILED|invalid" logs/v27a-fastboot-boot.txt; then
  ts "fastboot boot ACCEPTED (temporary boot)"
else
  ts "fastboot boot not supported: $(tail -1 logs/v27a-fastboot-boot.txt) — flashing instead"
  $FB flash boot out/boot-v27a-q706.img 2>&1 | tail -2 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 | tee -a $LOG
fi
T0=$(date +%s)

# 4. 观测 + 抢收（记录 adb 与 fastboot 双向变迁）
RESULT=UNKNOWN; LAST_AD="(init)"; LAST_FB=0; GRABBED=0
dump_evidence() {
  ts ">>> AUTHORIZED/BOOTED — grabbing evidence NOW"
  $ADB wait-for-device 2>/dev/null
  for t in 1 2 3; do
    $ADB shell "logcat -d -b all" > logs/v27a-logcat.txt 2>/dev/null
    [ -s logs/v27a-logcat.txt ] && break; sleep 1
  done
  $ADB shell "dmesg" > logs/v27a-dmesg.txt 2>/dev/null
  $ADB shell "cat /proc/cmdline" > logs/v27a-cmdline.txt 2>/dev/null
  $ADB shell "uname -a; ls /dev/watchdog* 2>&1; ps -A | head -80" > logs/v27a-ps.txt 2>/dev/null
  ts "evidence sizes: logcat=$(wc -c < logs/v27a-logcat.txt 2>/dev/null) dmesg=$(wc -c < logs/v27a-dmesg.txt 2>/dev/null)"
  GRABBED=1
}
while [ $(( $(date +%s) - T0 )) -lt 240 ]; do
  sleep 2
  LINE=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1)
  ST=$(echo "$LINE" | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  NOW=$(( $(date +%s) - T0 ))
  if [ "$ST" != "$LAST_AD" ] || { [ $FBL -gt 0 ] && [ $LAST_FB -eq 0 ]; }; then
    ts "T+${NOW}s | adb[$ST] | fb[$( [ $FBL -gt 0 ] && echo yes || echo '')]"
  fi
  LAST_AD=$ST; LAST_FB=$FBL
  case "$ST" in
    device) RESULT=BOOTED; [ $GRABBED -eq 0 ] && dump_evidence; sleep 30; dump_evidence; break ;;
    unauthorized) ts "still unauthorized — adb.secure=0 not honored?"; RESULT=UNAUTH; break ;;
  esac
  if [ $FBL -gt 0 ] && [ $NOW -gt 70 ]; then RESULT=FASTBOOT_FALLBACK; break; fi
done
ts "result: $RESULT (elapsed $(( $(date +%s) - T0 ))s)"

# 5. 恢复 + 收割
ts "restoring harvest13"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 40); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 | tee -a $LOG
else
  ts "WARN: no fastboot for restore — device may still be running v27a"
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
  ts "adb back: $($ADB shell uname -r | tr -d '\r')"
  sleep 15
  MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
  MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/ramoops-v27a-harvest.bin >/dev/null 2>&1
  ts "harvest: $(md5sum out/ramoops-v27a-harvest.bin 2>/dev/null | cut -c1-8)"
fi
ts "P50 DONE result=$RESULT grabbed=$GRABBED"
echo "$RESULT" > logs/v27a-ignition-result.txt
