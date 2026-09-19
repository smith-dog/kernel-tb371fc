#!/usr/bin/env python3
# p125-bm-enable-nodes.py (v2)
# 修复电池养护开关：HAL 需要 /sys/class/power_supply/battery/bm_enable
# 与 bm2_enable 节点（电池维护状态标志），GPL dump 的电池驱动未实现
# （stock 有）→ HAL "Open en_bm path error" → 开关永远打不开。
# 补齐两个标志节点（逐节点幂等挂载，避免 EEXIST 重试死锁；
# 权限由 init.target.rc 的 chown/chmod 0666 收敛为 root:system 0666）。
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/smb5-lib.c'

src = open(PATH).read()

anchor = '''static struct device_attribute dev_attr_lenovo_adapter_type =
	__ATTR(adapter_type, 0444, lenovo_adapter_type_show, NULL);
'''
new_attrs = anchor + '''
/* p125: battery maintenance state flags. The vendor HAL stores the
 * ZUI battery-maintenance toggle in these nodes (OSPINEL-617 chowns
 * them for the HAL); the GPL dump driver never created them, so the
 * HAL failed with "Open en_bm path error" and the toggle never stuck. */
static unsigned int lenovo_bm_enable_val;

static ssize_t lenovo_bm_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\\n", lenovo_bm_enable_val);
}

static ssize_t lenovo_bm_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (kstrtouint(buf, 10, &lenovo_bm_enable_val))
		return -EINVAL;
	pr_info("smb5-lib: battery maintenance set to %u\\n",
			lenovo_bm_enable_val);
	return count;
}

static struct device_attribute dev_attr_lenovo_bm_enable =
	__ATTR(bm_enable, 0644, lenovo_bm_enable_show,
					lenovo_bm_enable_store);

static struct device_attribute dev_attr_lenovo_bm2_enable =
	__ATTR(bm2_enable, 0644, lenovo_bm_enable_show,
					lenovo_bm_enable_store);
'''

old_attach = '''	struct power_supply *psy;
	bool batt_ok = false, usb_ok = false;

	psy = power_supply_get_by_name("battery");
	if (psy) {
		batt_ok = !device_create_file(&psy->dev,
				&dev_attr_lenovo_charge_disable);
		power_supply_put(psy);
	}
	psy = power_supply_get_by_name("usb");
	if (psy) {
		usb_ok = !device_create_file(&psy->dev,
				&dev_attr_lenovo_adapter_type);
		power_supply_put(psy);
	}'''

new_attach = '''	struct power_supply *psy;
	bool batt_ok = false, usb_ok = false;

	psy = power_supply_get_by_name("battery");
	if (psy) {
		if (!lenovo_abi_charge_disable_done)
			lenovo_abi_charge_disable_done =
				!device_create_file(&psy->dev,
				&dev_attr_lenovo_charge_disable);
		if (!lenovo_abi_bm_done)
			lenovo_abi_bm_done =
				!device_create_file(&psy->dev,
				&dev_attr_lenovo_bm_enable);
		if (!lenovo_abi_bm2_done)
			lenovo_abi_bm2_done =
				!device_create_file(&psy->dev,
				&dev_attr_lenovo_bm2_enable);
		batt_ok = lenovo_abi_charge_disable_done &&
			  lenovo_abi_bm_done && lenovo_abi_bm2_done;
		power_supply_put(psy);
	}
	psy = power_supply_get_by_name("usb");
	if (psy) {
		if (!lenovo_abi_adapter_done)
			lenovo_abi_adapter_done =
				!device_create_file(&psy->dev,
				&dev_attr_lenovo_adapter_type);
		usb_ok = lenovo_abi_adapter_done;
		power_supply_put(psy);
	}'''

bools_anchor = 'static int lenovo_battery_abi_retries;'
new_bools = '''static bool lenovo_abi_charge_disable_done;
static bool lenovo_abi_bm_done;
static bool lenovo_abi_bm2_done;
static bool lenovo_abi_adapter_done;
static int lenovo_battery_abi_retries;'''

if anchor not in src:
    print('ANCHOR NOT FOUND'); sys.exit(1)
if old_attach not in src:
    print('ATTACH BLOCK NOT FOUND'); sys.exit(1)
if bools_anchor not in src:
    print('RETRIES DECL NOT FOUND'); sys.exit(1)

src = src.replace(anchor, new_attrs, 1)
src = src.replace(old_attach, new_attach, 1)
src = src.replace(bools_anchor, new_bools, 1)
open(PATH, 'w').write(src)
print('p125 v2 applied: bm flags + idempotent per-attr attach')
