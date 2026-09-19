#!/system/bin/sh
echo "== refresh settings:"
settings get system peak_refresh_rate
settings get system min_refresh_rate
settings get global user_preferred_display_mode 2>/dev/null
echo "== zui refresh props:"
getprop | grep -iE 'refresh|fps|frame.?rate' | head -15
echo "== dmesg fps switch:"
dmesg | grep -iE 'fps_mode|switch.*fps|timing.*switch|SET_TIMING|60hz|90hz|120hz' | tail -20
echo "== sf:"
dumpsys SurfaceFlinger 2>/dev/null | grep -iE 'refresh-rate|present2paint|xpsync' | head -5
