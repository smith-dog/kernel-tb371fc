#!/usr/bin/env python3
"""p80 — apply Lenovo battery HAL ABI shim to smb5-lib.c (copy on Windows side).

Adds /sys/class/power_supply/battery/charge_disable (rw, maps to usb
input_suspend) and /sys/class/power_supply/usb/adapter_type (ro, vbus
presence 0/1). Fixes vendor.lenovo.hardware.battery@2.0-service error
loop -> hiz toggle -> plug/unplug uevent storm (all-CPU saturation).
"""
import sys, io

SRC = r"logs/smb5-lib.c.copy"
DST = r"logs/smb5-lib.c.patched"

SHIM = r'''
/* Lenovo TB371FC vendor battery HAL ABI shim.
 * vendor.lenovo.hardware.battery@2.0-service expects
 * /sys/class/power_supply/battery/charge_disable and
 * /sys/class/power_supply/usb/adapter_type. The public smblib
 * does not export them; the HAL error-loops (~12/s) and toggles
 * input suspend each cycle, producing a plug/unplug uevent storm
 * that saturates all CPUs (2026-09-18 diagnosis). charge_disable
 * maps to input suspend; adapter_type reports vbus presence (0/1),
 * which is what the HAL reads (expects len=1). */

static ssize_t lenovo_charge_disable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *usb;
	union power_supply_propval val = {0, };
	int rc, dis = 0;

	usb = power_supply_get_by_name("usb");
	if (usb) {
		rc = power_supply_get_property(usb,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
		if (!rc)
			dis = val.intval ? 1 : 0;
		power_supply_put(usb);
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", dis);
}

static ssize_t lenovo_charge_disable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *usb;
	union power_supply_propval val = {0, };
	int rc;

	if (kstrtoint(buf, 10, &val.intval))
		return -EINVAL;
	usb = power_supply_get_by_name("usb");
	if (!usb)
		return -ENODEV;
	rc = power_supply_set_property(usb,
			POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	power_supply_put(usb);
	return rc ? rc : count;
}

static struct device_attribute dev_attr_lenovo_charge_disable =
	__ATTR(charge_disable, 0644, lenovo_charge_disable_show,
					lenovo_charge_disable_store);

static ssize_t lenovo_adapter_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *usb;
	union power_supply_propval val = {0, };
	int rc, type = 0;

	usb = power_supply_get_by_name("usb");
	if (usb) {
		rc = power_supply_get_property(usb,
				POWER_SUPPLY_PROP_PRESENT, &val);
		if (!rc)
			type = val.intval ? 1 : 0;
		power_supply_put(usb);
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", type);
}

static struct device_attribute dev_attr_lenovo_adapter_type =
	__ATTR(adapter_type, 0444, lenovo_adapter_type_show, NULL);

static int lenovo_battery_abi_retries;

static void lenovo_battery_abi_attach(struct work_struct *work);
static DECLARE_DELAYED_WORK(lenovo_battery_abi_work,
				lenovo_battery_abi_attach);
static int lenovo_battery_abi_attached;

static void lenovo_battery_abi_attach(struct work_struct *work)
{
	struct power_supply *psy;
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
	}
	if (batt_ok && usb_ok) {
		pr_info("smb5-lib: Lenovo battery HAL ABI shim attached\n");
		return;
	}
	if (lenovo_battery_abi_retries++ < 20)
		schedule_delayed_work(&lenovo_battery_abi_work,
				msecs_to_jiffies(5000));
}
'''

SCHED = '''	if (!lenovo_battery_abi_attached) {
		lenovo_battery_abi_attached = 1;
		schedule_delayed_work(&lenovo_battery_abi_work,
				msecs_to_jiffies(8000));
	}

	return rc;
}

int smblib_deinit(struct smb_charger *chg)'''

ANCHOR_TAIL = '''	return rc;
}

int smblib_deinit(struct smb_charger *chg)'''
ANCHOR_INIT = "int smblib_init(struct smb_charger *chg)"

src = io.open(SRC, "r", encoding="utf-8", newline="").read()
assert src.count(ANCHOR_INIT) == 1, "smblib_init anchor not unique"
assert src.count(ANCHOR_TAIL) == 1, "smblib_init tail anchor not unique"
assert "lenovo_battery_abi_attach" not in src, "shim already applied"

# bottom-up insertion
src = src.replace(ANCHOR_TAIL, SCHED, 1)
src = src.replace(ANCHOR_INIT, SHIM + "\n" + ANCHOR_INIT, 1)

io.open(DST, "w", encoding="utf-8", newline="").write(src)
print("patched ->", DST)
print("lines:", src.count("\n"))
