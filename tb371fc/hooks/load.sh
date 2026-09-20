#!/system/bin/sh
# load.sh — TB371FC vendor 模块加载（LKM 版）：ksu.ko 最先，再模块集
# active_stack 决定 dlkm / dlkm-lenovo（回退：echo dlkm > active_stack）
MODDIR=${0%/*}
STACK=$(cat "$MODDIR/active_stack" 2>/dev/null | tr -d ' \r\n')
case "$STACK" in
  dlkm|dlkm-lenovo) ;;
  *) STACK=dlkm ;;
esac
DLKM="$MODDIR/$STACK"
LOG=$DLKM/boot_load.log
(
# KernelSU LKM first — root 供后续 insmod128/依赖使用
if [ -f "$MODDIR/ksu.ko" ]; then
  insmod "$MODDIR/ksu.ko" 2>>$LOG && echo "=== ksu.ko loaded ===" >> $LOG
fi
sleep 5
echo "=== dlkm load [$STACK] $(date) ===" >> $LOG
insmod $DLKM/hq_shim.ko 2>>$LOG
sh $DLKM/load_ordered.sh >> $LOG 2>&1
echo 1 > /sys/kernel/boot_adsp/boot 2>>$LOG
echo "=== cards:" >> $LOG
cat /proc/asound/cards >> $LOG 2>&1
) &
$MODDIR/fwdbg.sh &
