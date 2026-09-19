#!/bin/bash
# p54 — v27d 点火 + harvest14 原始收割:
#   v27d   = Image-v27 + tail-ramoops.bin (compatible=ramoops) + 原版 ramdisk
#            → console-ramoops 持续记录 dmesg, 死亡后残留于 0x27E000000
#   harvest14 = Apatch 内核(=harvest13 同款) + tail-ramoops.bin
#            → stock 侧 no-map 同区域 → 不被页缓存覆盖 → KPM 读到原始死亡日志
# 流程: 刷 v27d → 观测(抢收) → 死亡 → 刷 harvest14 → 拉取+解析 → 恢复 harvest13
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27d-ignition-timeline.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P54 START"
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27q706 \
    out/boot-v27d-q706.img out/dtbs/tail-ramoops.bin 2>&1 | tail -1 | tee -a $LOG
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Apatch-kernel.bin \
    out/boot-harvest14.img out/dtbs/tail-ramoops.bin 2>&1 | tail -1 | tee -a $LOG

$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL: no adb"; exit 1; }
ts "device online: $($ADB shell uname -r | tr -d '\r')"
$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB devices 2>/dev/null | grep -q fastboot || { ts "FATAL no fastboot"; exit 1; }

ts "flashing v27d"
$FB flash boot out/boot-v27d-q706.img 2>&1 | tail -1 | tee -a $LOG
$FB reboot 2>&1 | tail -1 >/dev/null
T0=$(date +%s)

grab() {
  {
    echo "===== grab @T+$(( $(date +%s) - T0 ))s ====="
    $ADB shell "cat /proc/cmdline; ls /sys/fs/pstore/ 2>&1; dmesg | tail -150" 2>/dev/null
    echo "--- logcat ---"
    $ADB shell "logcat -d -b all 2>/dev/null | tail -200" 2>/dev/null
  } >> logs/v27d-evidence.txt 2>&1
}
: > logs/v27d-evidence.txt
RESULT=UNKNOWN; LAST_AD="(i)"; LAST_FB=0; NGRAB=0
while [ $(( $(date +%s) - T0 )) -lt 300 ]; do
  sleep 2
  LINE=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1)
  ST=$(echo "$LINE" | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  NOW=$(( $(date +%s) - T0 ))
  if [ "$ST" != "$LAST_AD" ] || { [ $FBL -gt 0 ] && [ $LAST_FB -eq 0 ]; }; then
    ts "T+${NOW}s | adb[$ST] | fb[$( [ $FBL -gt 0 ] && echo yes || echo '')]"
  fi
  LAST_AD=$ST; LAST_FB=$FBL
  if [ "$ST" = "device" ]; then RESULT=BOOTED_AUTHORIZED; grab; NGRAB=$((NGRAB+1)); fi
  if [ $FBL -gt 0 ] && [ $NOW -gt 70 ]; then [ "$RESULT" = "UNKNOWN" ] && RESULT=FASTBOOT_FALLBACK; break; fi
done
ts "v27d result: $RESULT grabs=$NGRAB (T+$(( $(date +%s) - T0 ))s)"

# 收割: 刷 harvest14, KPM 在 post-fs-data 落盘原始区域
ts "flashing harvest14 (evidence-preserving recovery)"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 40); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest14.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 >/dev/null
else
  ts "WARN: no fastboot"
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
  ts "harvest14 up: $($ADB shell uname -r | tr -d '\r')"
  sleep 25
  MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
  MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/ramoops-v27d-harvest.bin >/dev/null 2>&1
  ts "harvest pulled: $(md5sum out/ramoops-v27d-harvest.bin 2>/dev/null | cut -c1-8)"
fi

# 恢复日常 harvest13
ts "restoring harvest13 (daily)"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 40); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 >/dev/null
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
[ "$($ADB get-state 2>/dev/null)" = "device" ] && ts "daily restored: $($ADB shell uname -r | tr -d '\r')"
ts "P54 DONE result=$RESULT"
echo "$RESULT" > logs/v27d-ignition-result.txt
