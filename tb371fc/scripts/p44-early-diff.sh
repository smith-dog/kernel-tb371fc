#!/bin/bash
# p44 — 三方早期段比对: bjsl08 / 干净stock(fw) / v24
# 1) 从固件 boot.img 提取干净 stock 内核  2) p31 提取其 kallsyms
# 3) p44-early-diff.py 三方比对
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p44-early-diff.log 2>&1
echo "=== P44 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel

echo "=== 1. 提取干净 stock 内核 (fw boot.img) ==="
python3 - <<'PYEOF'
import struct
p = '/mnt/d/work/code-work/project/tb371fc-kernel/fw/TB371FC_ZUI_16.0.474/image/boot.img'
o = '/mnt/d/work/code-work/project/tb371fc-kernel/out/kernel-stock-fw.bin'
d = open(p, 'rb').read(64)
assert d[:8] == b'ANDROID!', d[:8]
ksz, psz = struct.unpack_from('<I', d, 8)[0], struct.unpack_from('<I', d, 36)[0]
print(f'kernel_size={ksz} page_size={psz}')
f = open(p, 'rb')
f.seek(psz)
open(o, 'wb').write(f.read(ksz))
print('wrote', o)
PYEOF
ls -la $BASE/out/kernel-stock-fw.bin

echo "=== 2. p31 提取 stock kallsyms ==="
python3 $BASE/scripts/p31-kallsyms-parse.py $BASE/out/kernel-stock-fw.bin $BASE/out/kallsyms-stock-fw.txt 2>&1 | tail -3

echo "=== 3. 三方早期段比对 ==="
python3 $BASE/scripts/p44-early-diff.py

echo "=== P44 DONE $(date) ==="
