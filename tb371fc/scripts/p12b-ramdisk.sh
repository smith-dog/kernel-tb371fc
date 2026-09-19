#!/bin/bash
# Fix mini-ramdisk init: mount pstore before copying console-ramoops
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p12b.log 2>&1
echo START
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out

cd $BASE/initrd || exit 1
cat > rootfs/init <<'EOS'
#!/busybox sh
/busybox mkdir -p /proc /sys /dev /mnt
/busybox mount -t proc proc /proc
/busybox mount -t sysfs sysfs /sys
/busybox mount -t tmpfs tmpfs /dev
/busybox mkdir -p /sys/fs/pstore
/busybox mount -t pstore pstore /sys/fs/pstore
/busybox mknod /dev/persist b 8 2
/busybox mount -t ext4 /dev/persist /mnt
if [ $? = 0 ]; then
  if [ ! -f /mnt/klog-console.txt ]; then
    /busybox cp /sys/fs/pstore/console-ramoops* /mnt/ 2>/dev/null
    /busybox cp /sys/fs/pstore/dmesg-ramoops* /mnt/ 2>/dev/null
    /busybox ls /sys/fs/pstore/ > /mnt/klog-pstore-listing.txt
  fi
  /busybox dmesg > /mnt/klog-readerboot.txt 2>/dev/null
  /busybox sync
  /busybox umount /mnt
fi
/busybox sync
/busybox reboot -f bootloader
EOS
chmod 755 rootfs/init
(cd rootfs && find . | cpio -o -H newc | gzip > $OUT/mini-ramdisk2b.img)
ls -la $OUT/mini-ramdisk2b.img

python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-van $OUT/boot-v7-reader2.img $OUT/dtbs/tail-new.bin $OUT/mini-ramdisk2b.img
echo READER2B_DONE
