#!/system/bin/sh
echo "== boot_completed: $(getprop sys.boot_completed)"
echo "== rescue_level: $(getprop persist.sys.rescue_level)"
echo "== remoteproc/adsp:"
dmesg | grep -iE 'remoteproc|adsp|cdsp|slpi' | tail -15
echo "== display:"
dmesg | grep -iE 'dfps|dsi.*error|ESD|panel' | tail -15
echo "== ss pid:"
ps -A | grep system_server | awk '{print $2, $NF}'
