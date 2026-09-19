#!/bin/bash
for i in $(seq 0 13); do
  if [ -s /tmp/dtbo_$i.dtb ]; then
    dtc -I dtb -O dts -o /tmp/dtbo_$i.dts /tmp/dtbo_$i.dtb 2>/dev/null
    echo "decompiled $i: $(wc -c < /tmp/dtbo_$i.dts)"
  else
    echo "missing dtb $i"
  fi
done
grep -l 'nt36532\|tianma' /tmp/dtbo_*.dts
echo ---STATUS--- $(grep -c '' /tmp/dtbo_*.dts | head -20)
