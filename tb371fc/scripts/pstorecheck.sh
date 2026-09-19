#!/system/bin/sh
echo "== pstore:"
ls -la /sys/fs/pstore/ 2>/dev/null
echo "== last reboot reason props:"
getprop | grep -iE 'boot.reason|rescue' | head
echo "== last_kmsg tail:"
tail -c 3000 /sys/fs/pstore/console-ramoops-0 2>/dev/null
