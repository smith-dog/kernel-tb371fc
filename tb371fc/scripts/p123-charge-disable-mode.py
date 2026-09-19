#!/usr/bin/env python3
# p123-fp-charge-disable-mode.py
# 修复充电保护风暴：p80 的 charge_disable shim 属性为 0644 root:root，
# 而 Lenovo 电池 HAL 以 system 身份运行（无法写入），开启充电保护后
# HAL 高速错误循环 + hiz 反复拨动。init.target.rc 的 chmod 0666 在
# post-fs-data 执行时节点尚未创建（p80 延迟工作 ~8-13s 才挂载属性），
# 永远错过。本补丁把属性模式改为 0666，与 init.target.rc 的意图一致；
# SELinux (vendor_sysfs_battery_supply write) 仍拦截普通应用域。
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/smb5-lib.c'

src = open(PATH).read()
old = '''static struct device_attribute dev_attr_lenovo_charge_disable =
	__ATTR(charge_disable, 0644, lenovo_charge_disable_show,
					lenovo_charge_disable_store);'''
new = '''static struct device_attribute dev_attr_lenovo_charge_disable =
	__ATTR(charge_disable, 0666, lenovo_charge_disable_show,
					lenovo_charge_disable_store);'''

if old not in src:
    print('PATTERN NOT FOUND (already patched?)'); sys.exit(1)
open(PATH, 'w').write(src.replace(old, new))
print('p123 applied: charge_disable attr 0644 -> 0666')
