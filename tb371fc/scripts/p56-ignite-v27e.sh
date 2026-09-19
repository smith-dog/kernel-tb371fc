#!/bin/bash
# p56 — v27e 点火: v27 + tail-new(原字节, 无 compatible) + cmdline ramoops(mem_type=1)
# 判别实验: 与 v27 唯一差异 = cmdline 追加 ramoops 参数
#   - v27e 活到 adbd(blip) ⇒ 挂死(v27d)在 DTB 特有时序, probe 代码本身安全
#   - v27e 无 blip 直接死 ⇒ probe 代码路径本身在早期挂死(与 v27d 同因)
#   - v27e 完整开机 ⇒ ramoops 生效 + 死因恰在 pstore 建立前消失的可能性极低
# 收割: 死亡后 console-ramoops 残留 0x27E000000(v27e DTB 无 compatible 但
#   cmdline mem_size 使 postcore 时 probe 成功, 区域仍是 no-map? 否——tail-new
#   的 ramoops 节点有 no-map 属性, v27e 同样 no-map 保留它(不需 compatible),
#   故区域受保护; harvest14(stock+tail-ramoops) 收割时区域同样被 stock 保留
#   ⇒ 页缓存不再覆盖 ⇒ KPM 读到原始死亡日志)
set -u
cd /d/work/code-work/project/tb371fc-kernel
ADB=./platform-tools/adb.exe
FB=./platform-tools/fastboot.exe
SERIAL=HA1ZXE2M
LOG=logs/v27e-ignition-timeline.txt
: > $LOG
ts() { echo "[$(date +%H:%M:%S)] $*" | tee -a $LOG; }

ts "P56 v27e ignition start"
python tools/repack_boot.py apatch_patched_11266_0.13.5_clte.img out/Image-v27q706 \
    out/boot-v27e-q706.img out/dtbs/tail-new.bin "" \
    "ramoops.mem_address=0x27E000000 ramoops.mem_size=0x200000 ramoops.console_size=0x180000" 2>&1 | tail -2 | tee -a $LOG
[ -f out/boot-v27e-q706.img ] || { ts FATAL_REPACK; exit 1; }

$ADB get-state 2>/dev/null | grep -q device || { ts "FATAL: no adb"; exit 1; }
ts "device online: $($ADB shell uname -r | tr -d '\r')"
$ADB reboot bootloader
for i in $(seq 1 30); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
$FB devices 2>/dev/null | grep -q fastboot || { ts "FATAL no fastboot"; exit 1; }
$FB flash boot out/boot-v27e-q706.img 2>&1 | tail -1 | tee -a $LOG
$FB reboot 2>&1 | tail -1 >/dev/null
T0=$(date +%s)

grab() {
  {
    echo "===== grab @T+$(( $(date +%s) - T0 ))s ====="
    $ADB shell "cat /proc/cmdline | tr ' ' '\n' | grep ramoops; ls /sys/fs/pstore/ 2>&1; uname -a" 2>/dev/null
    $ADB shell "su -c 'cat /sys/fs/pstore/console-ramoops 2>/dev/null | tail -100'" 2>/dev/null
  } >> logs/v27e-evidence.txt 2>&1
}
: > logs/v27e-evidence.txt
RESULT=UNKNOWN; LAST_AD="(i)"; LAST_FB=0; NGRAB=0
while [ $(( $(date +%s) - T0 )) -lt 200 ]; do
  sleep 2
  LINE=$($ADB devices 2>/dev/null | grep "$SERIAL" | head -1)
  ST=$(echo "$LINE" | awk '{print $2}'); [ -z "$ST" ] && ST="(none)"
  FBL=$($FB devices 2>/dev/null | grep -c fastboot)
  NOW=$(( $(date +%s) - T0 ))
  if [ "$ST" != "$LAST_AD" ] || { [ $FBL -gt 0 ] && [ $LAST_FB -eq 0 ]; }; then
    ts "T+${NOW}s | adb[$ST] | fb[$( [ $FBL -gt 0 ] && echo yes || echo '')]"
  fi
  LAST_AD=$ST; LAST_FB=$FBL
  if [ "$ST" = "device" ]; then RESULT=BOOTED_AUTHORIZED; grab; NGRAB=$((NGRAB+1)); sleep 25; grab; NGRAB=$((NGRAB+1)); break; fi
  # 140s 无任何信号 = 疑似 v27d 式挂死 → 立即换 harvest14 保证据并提示用户
  if [ "$ST" = "(none)" ] && [ $FBL -eq 0 ] && [ $NOW -gt 140 ]; then
    RESULT=HANG_NO_SIGNAL; break
  fi
  if [ $FBL -gt 0 ] && [ $NOW -gt 70 ]; then [ "$RESULT" = "UNKNOWN" ] && RESULT=FASTBOOT_FALLBACK; break; fi
done
ts "v27e result: $RESULT grabs=$NGRAB (T+$(( $(date +%s) - T0 ))s)"

if [ "$RESULT" = "HANG_NO_SIGNAL" ]; then
  ts "HANG detected — trying reboot into fastboot via 60s wait (watchdog may recover)"
  for i in $(seq 1 60); do sleep 10; $FB devices 2>/dev/null | grep -q fastboot && { ts "fastboot recovered"; break; } || ts "  no signal ($i/60)"; done
fi

ts "recovery: flashing harvest14 (evidence-preserving)"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 45); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest14.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 >/dev/null
else
  ts "CRITICAL: no fastboot — device hung like v27d. User intervention needed."
  cd /d/work/code-work && python call_me.py "平板再次挂死无信号 请长按电源关机 再 音量减+电源 进fastboot 我会自动恢复" 2>&1 | head -1
  exit 9
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
if [ "$($ADB get-state 2>/dev/null)" = "device" ]; then
  ts "harvest14 up: $($ADB shell uname -r | tr -d '\r')"
  sleep 25
  MSYS_NO_PATHCONV=1 $ADB shell "su -c 'chmod 644 /data/local/tmp/ramoops-prev.bin'" 2>/dev/null
  MSYS_NO_PATHCONV=1 $ADB pull /data/local/tmp/ramoops-prev.bin out/ramoops-v27e-harvest.bin >/dev/null 2>&1
  ts "harvest: $(md5sum out/ramoops-v27e-harvest.bin 2>/dev/null | cut -c1-8) size=$(wc -c < out/ramoops-v27e-harvest.bin 2>/dev/null)"
fi

ts "restoring harvest13"
$ADB reboot bootloader 2>/dev/null
for i in $(seq 1 45); do sleep 2; $FB devices 2>/dev/null | grep -q fastboot && break; done
if $FB devices 2>/dev/null | grep -q fastboot; then
  $FB flash boot out/boot-harvest13.img 2>&1 | tail -1 | tee -a $LOG
  $FB set_active a 2>&1 | tail -1 | tee -a $LOG
  $FB reboot 2>&1 | tail -1 >/dev/null
fi
for i in $(seq 1 60); do sleep 3; [ "$($ADB get-state 2>/dev/null)" = "device" ] && break; done
[ "$($ADB get-state 2>/dev/null)" = "device" ] && ts "daily restored: $($ADB shell uname -r | tr -d '\r')"
ts "P56 DONE result=$RESULT"
echo "$RESULT" > logs/v27e-ignition-result.txt
