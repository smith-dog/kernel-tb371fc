#!/system/bin/sh
# p116 device-side loader — insmod /data/local/tmp/dlkm modules in
# modules.load order with dependency resolution via modules.dep.
# Usage: su -c 'sh /data/local/tmp/dlkm/load.sh'
DIR=/data/local/tmp/dlkm
DEP="$DIR/modules.dep"
LOAD="$DIR/modules.load"
LOG=/data/local/tmp/dlkm/load.log

loaded() { grep -q "^$1 " /proc/modules 2>/dev/null || grep -q "^$1$" /proc/modules 2>/dev/null; }

insmod_rec() {
	m="$1"
	base=$(basename "$m")
	loaded "$base" && return 0
	# load its deps first (dep line: "a.ko: b.ko c.ko")
	line=$(grep -F "$base:" "$DEP" | head -1)
	deps=${line#*: }
	for d in $deps; do
		[ "$d" != "$line" ] || break
		insmod_rec "$DIR/$(basename "$d")"
	done
	if insmod "$DIR/$base" 2>>"$LOG"; then
		echo "OK  $base" >> "$LOG"
	else
		echo "ERR $base" >> "$LOG"
	fi
}

: > "$LOG"
while read -r m; do
	[ -n "$m" ] || continue
	insmod_rec "$m"
done < "$LOAD"

echo "=== loaded:" >> "$LOG"
wc -l < /proc/modules >> "$LOG"
cat "$LOG"
