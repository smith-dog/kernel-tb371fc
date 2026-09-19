#!/usr/bin/env python3
"""p85 — charge_disable shim v2: accept-and-remember, no power state writes.

The v1 mapping (charge_disable -> usb input_suspend) made the vendor HAL's
setUsbSupplyDisabled physically drop the USB input; the PD voter reasserted
3A ~15ms later -> infinite plug/unplug vote war (healthd/battery saver/
refresh-rate storms). v2 accepts and remembers the value only.
"""
P = "/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/smb5-lib.c"
NL = chr(10)
BSN = chr(92) + "n"
T = chr(9)

START = "static ssize_t lenovo_charge_disable_show(struct device *dev,"
END = "static struct device_attribute dev_attr_lenovo_charge_disable ="

NEW = (
    "static int lenovo_charge_disable_val;" + NL + NL +
    "static ssize_t lenovo_charge_disable_show(struct device *dev," + NL +
    T + T + "struct device_attribute *attr, char *buf)" + NL +
    "{" + NL +
    T + "return scnprintf(buf, PAGE_SIZE, \"%d" + BSN + "\", lenovo_charge_disable_val);" + NL +
    "}" + NL + NL +
    "static ssize_t lenovo_charge_disable_store(struct device *dev," + NL +
    T + T + "struct device_attribute *attr," + NL +
    T + T + "const char *buf, size_t count)" + NL +
    "{" + NL +
    T + "/* Accept and remember only: mapping this to input suspend made" + NL +
    T + " * the vendor HAL fight the PD voter at 15ms cadence (charger" + NL +
    T + " * plug/unplug storm). Real charge-disable semantics deferred. */" + NL +
    T + "if (kstrtoint(buf, 10, &lenovo_charge_disable_val))" + NL +
    T + T + "return -EINVAL;" + NL +
    T + "return count;" + NL +
    "}" + NL + NL +
    "static struct device_attribute dev_attr_lenovo_charge_disable ="
)

src = open(P).read()
i = src.index(START)
j = src.index(END)
src = src[:i] + NEW + src[j + len(END):]
open(P, "w").write(src)
print("shim v2 applied, replaced", j - i, "bytes")
