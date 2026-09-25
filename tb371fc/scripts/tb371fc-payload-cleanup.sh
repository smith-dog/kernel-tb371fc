#!/system/bin/sh
# tb371fc-payload-cleanup.sh — TB371FC 旧载荷包清除脚本(社区版,2026-09-26)
#
# 适用:已刷入 2026-09-26 新内核(n96c/#159 或更新)的设备。
# 作用:把旧载荷包(tb371fc-dlkm + 98-dlkm 钩子)整体【移动】到
#       /sdcard/Download/tb371fc-payload-removed-<时间>/,不使用 rm,
#       文件全部保留,可随时移回原位恢复。
#
# 用法(设备已 root,adb 可用):
#   adb push tb371fc-payload-cleanup.sh /data/local/tmp/
#   adb shell "su -c 'sh /data/local/tmp/tb371fc-payload-cleanup.sh'"
#
# 原理:新内核已把音频/WiFi 等全部驱动内建,并由内核自行触发 WiFi,
# 旧载荷包(ko 模块+开机钩子)不再有任何用处,残留只会在每次开机时
# 尝试加载一堆注定失败的旧模块。

DEST_BASE=/sdcard/Download/tb371fc-payload-removed

# --- 0. root 检查 ---
if [ "$(id -u)" != "0" ]; then
  echo "[X] 请用 root 运行: su -c sh $0"
  exit 1
fi

# --- 1. 回收目录(优先 /sdcard/Download,不可写则退 /data/local/tmp)---
TS=$(date +%Y%m%d-%H%M%S)
D="$DEST_BASE-$TS"
mkdir -p "$D" 2>/dev/null || { D="/data/local/tmp/tb371fc-payload-removed-$TS"; mkdir -p "$D"; }
if [ ! -d "$D" ]; then
  echo "[X] 无法创建回收目录,放弃(未做任何改动)"
  exit 1
fi

# --- 2. 内核检查:旧内核删载荷会失去 WiFi/音频,必须拦截 ---
if ! grep -q wlan_boot_self_trigger /proc/kallsyms 2>/dev/null; then
  echo "[X] 当前内核没有 wlan 自触发功能 => 仍是旧内核。"
  echo "    旧内核依赖载荷包提供 WiFi/音频,现在删除会断功能,脚本拒绝执行。"
  echo "    请先刷入 2026-09-26 的新内核(boot-v27n96c-kspatched-flash.img 或更新),"
  echo "    重启后再运行本脚本。本次未做任何改动。"
  rmdir "$D" 2>/dev/null
  exit 1
fi
if ! grep -q kona_tdm_be_ops /proc/kallsyms 2>/dev/null; then
  echo "[X] 当前内核音频 machine 驱动未内建 => 内核过旧(介于新旧之间)。"
  echo "    请确认刷入的是 n96c/#159 及之后的 boot 镜像。本次未做任何改动。"
  rmdir "$D" 2>/dev/null
  exit 1
fi
echo "[OK] 新内核确认(驱动全内建 + wlan 内核自触发),开始清理"

MOVED=0

# --- 3. 先停开机钩子(防止下次开机再尝试加载旧模块)---
for H in /data/adb/post-fs-data.d/*; do
  [ -f "$H" ] || continue
  case "$H" in
    *dlkm*|*tb371fc*) ;;
    *) grep -q "tb371fc-dlkm" "$H" 2>/dev/null || continue ;;
  esac
  mv "$H" "$D/hook-$(basename "$H")" || { echo "[X] 移动 $H 失败,中止"; exit 1; }
  MOVED=$((MOVED+1))
  echo "[OK] 钩子已停并移出: $H"
done

# --- 4. 挪走载荷目录 ---
if [ -d /data/adb/tb371fc-dlkm ]; then
  mv /data/adb/tb371fc-dlkm "$D/tb371fc-dlkm" || { echo "[X] 移动载荷目录失败,中止"; exit 1; }
  MOVED=$((MOVED+1))
  echo "[OK] 载荷目录已移出: /data/adb/tb371fc-dlkm"
fi

# --- 5. 结果 ---
if [ "$MOVED" = "0" ]; then
  echo "[i] 未找到载荷包痕迹(可能已清理过),无需处理。"
  rmdir "$D" 2>/dev/null
  exit 0
fi

echo ""
echo "=== 清理完成:共移动 $MOVED 项 -> $D ==="
echo "所有文件已保留(未删除),设备使用正常一段时间后,可在文件管理器里"
echo "自行删除 Download 下的 tb371fc-payload-removed-* 目录。"
echo ""
echo "请立即重启手机。重启后预期状态:"
echo "  - 开机约 15 秒内 WiFi 自动打开并连网(内核自触发)"
echo "  - 扬声器/录音/耳机正常"
echo "  - 终端执行 lsmod 应只有 ksu 一行"
