#!/usr/bin/env python3
# p126-vintf-version.py
# 弹窗根治实验（方案A-1）：SUBLEVEL 157 → 198，使内核版本 4.19.198 ≥ FCM6
# 要求的 4.19.191（vendor target-level=5 时 framework 选表规则另验）。
# 注意：UTS_RELEASE 变化后，/data/adb/tb371fc-dlkm/dlkm 的 40 个 Lenovo 模块
# vermagic 需同步重打（4.19.157-perf+ → 4.19.198-perf+），由设备端脚本处理。
import sys, re

MK = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/Makefile'

s = open(MK).read()
m = re.search(r'^SUBLEVEL = (\d+)', s, re.M)
if not m:
    print('SUBLEVEL NOT FOUND'); sys.exit(1)
old = m.group(1)
if old == '198':
    print('already 198'); sys.exit(0)
s = re.sub(r'^SUBLEVEL = \d+', 'SUBLEVEL = 198', s, count=1, flags=re.M)
open(MK, 'w').write(s)
print(f'p126: SUBLEVEL {old} -> 198')
