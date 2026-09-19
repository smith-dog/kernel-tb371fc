#!/usr/bin/env python3
# p128-modversions-on.py
# 弹窗根治最后一步：CONFIG_MODVERSIONS=y（VINTF matrix5 硬性要求 =y，
# 缺失即 fail）。配套：Lenovo vendor 模块用 insmod128（finit_module +
# MODULE_INIT_IGNORE_MODVERSIONS）加载以跳过 CRC 比对（vermagic 检查保留）。
import sys

CONF = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/.config'

s = open(CONF).read()
if 'CONFIG_MODVERSIONS=y' in s:
    print('MODVERSIONS already y'); sys.exit(0)
if '# CONFIG_MODVERSIONS is not set' in s:
    s = s.replace('# CONFIG_MODVERSIONS is not set', 'CONFIG_MODVERSIONS=y', 1)
else:
    s = s.replace('CONFIG_SYSVIPC=y', 'CONFIG_SYSVIPC=y\nCONFIG_MODVERSIONS=y', 1) \
        if 'CONFIG_SYSVIPC=y' in s else s + '\nCONFIG_MODVERSIONS=y\n'
open(CONF, 'w').write(s)
print('p128: MODVERSIONS=y set')
