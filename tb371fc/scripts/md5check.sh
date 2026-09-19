#!/system/bin/sh
echo "== md5 boot_a:"
md5sum /dev/block/bootdevice/by-name/boot_a
echo "== md5 boot_b:"
md5sum /dev/block/bootdevice/by-name/boot_b
echo "== P100 prints:"
dmesg | grep -c 'P100'
dmesg | grep 'P100' | tail -3
echo "== current slot:"
getprop ro.boot.slot_suffix
