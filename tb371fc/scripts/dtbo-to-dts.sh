#!/bin/bash
set -e
D=/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbo-dts
mkdir -p $D
python3 /mnt/d/work/code-work/project/tb371fc-kernel/scripts/split_dtbo.py > /mnt/d/work/code-work/project/tb371fc-kernel/logs/split.log 2>&1 || cat /mnt/d/work/code-work/project/tb371fc-kernel/logs/split.log
sed -i "s|/tmp/dtbo_%d.dtb|$D/dtbo_%d.dtb|" /mnt/d/work/code-work/project/tb371fc-kernel/scripts/split_dtbo.py
python3 /mnt/d/work/code-work/project/tb371fc-kernel/scripts/split_dtbo.py
for i in $(seq 0 13); do
  if [ -s $D/dtbo_$i.dtb ]; then
    dtc -I dtb -O dts -o $D/dtbo_$i.dts $D/dtbo_$i.dtb 2>/dev/null
    echo "decompiled $i: $(wc -c < $D/dtbo_$i.dts)"
  fi
done
echo === PANEL MATCHES ===
grep -l 'spinel_tianma_nt36532' $D/*.dts || echo none
