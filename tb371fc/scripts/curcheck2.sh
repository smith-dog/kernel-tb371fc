#!/system/bin/sh
echo "== uname:"; uname -v
echo "== P108/POMS:"
dmesg | grep -E 'P108|POMS|P96' | head -10
echo "== avdd context:"
dmesg | grep -B3 -A3 'display_panel_avdd'
echo "== power key / sleep:"
dmesg | grep -iE 'powerkey|power key|suspend|PM:' | tail -10
echo "== uptime:"; uptime
echo "== bootanim:"; ps -A | grep -E 'bootanim|systemui' | awk '{print $2, $NF}'
