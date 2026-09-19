#!/bin/bash
# Build mini rescue ramdisk v3 (dd pstore console into raw logdump partition) + reader image
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-reader3.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out

cd $BASE/initrd || { mkdir -p $BASE/initrd; cd $BASE/initrd; }
cat > rootfs/init <<'EOS'
#!/busybox sh
/busybox mkdir -p /proc /sys /dev
/busybox mount -t proc proc /proc
/busybox mount -t sysfs sysfs /sys
/busybox mount -t tmpfs tmpfs /dev
/busybox mknod /dev/logdump b 259 38
/busybox mkdir -p /sys/fs/pstore
/busybox mount -t pstore pstore /sys/fs/pstore
C=/sys/fs/pstore/console-ramoops-0
[ -f $C ] || C=/sys/fs/pstore/console-ramoops
if [ -f $C ]; then
  /busybox dd if=$C of=/dev/logdump bs=4096 count=400 conv=notrunc 2>/dev/null
fi
/busybox echo "===CURRENT-BOOT-DMESG===" | /busybox dd of=/dev/logdump bs=4096 seek=400 conv=notrunc 2>/dev/null
/busybox dmesg > /tmp/dmesg.txt 2>/dev/null
/busybox dd if=/tmp/dmesg.txt of=/dev/logdump bs=4096 seek=401 conv=notrunc 2>/dev/null
/busybox sync
/busybox reboot -f bootloader
EOS
chmod 755 rootfs/init
rm -f $OUT/mini-ramdisk3.img
(cd rootfs && find . | cpio -o -H newc | gzip > $OUT/mini-ramdisk3.img)
ls -la $OUT/mini-ramdisk3.img

python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-200 $OUT/boot-v8-reader.img $OUT/dtbs/tail-new.bin $OUT/mini-ramdisk3.img
echo READER3_DONE
