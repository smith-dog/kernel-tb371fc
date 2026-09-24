#!/usr/bin/env python3
"""p187 — TB371FC USB host VBUS.

Lenovo's smb5 VBUS regulator ops drive an external boost through a DT GPIO
("qcom,gpio_boost_en") that the shipped TB371FC devicetree never provides, so
gpio_boost_en == -2 and gpio_direction_output() always fails -> no 5V on the
port in Type-C source mode -> no USB peripheral ever enumerates.

Fall back to the SMB5's own OTG boost (DCDC_CMD_OTG_REG.OTG_EN), i.e. the
Qualcomm register write Lenovo commented out, whenever no boost GPIO exists.
smblib_vbus_regulator_is_enabled() already reads that very register.
"""
import shutil
import sys

path = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/power/supply/qcom/smb5-lib.c"

OLD_EN = """	rc = gpio_direction_output(chg->gpio_boost_en, 1);
\t//rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, OTG_EN_BIT);
"""
NEW_EN = """\tif (gpio_is_valid(chg->gpio_boost_en)) {
\t\trc = gpio_direction_output(chg->gpio_boost_en, 1);
\t} else {
\t\t/* TB371FC DT has no qcom,gpio_boost_en: use the SMB5 OTG boost. */
\t\trc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT,
\t\t\t\t\t OTG_EN_BIT);
\t}
"""

OLD_DIS = """	rc = gpio_direction_output(chg->gpio_boost_en, 0);
\t//rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
"""
NEW_DIS = """\tif (gpio_is_valid(chg->gpio_boost_en)) {
\t\trc = gpio_direction_output(chg->gpio_boost_en, 0);
\t} else {
\t\trc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
\t}
"""

src = open(path, encoding="utf-8").read()
if "p187" in src or "enabling PMIC internal OTG boost" in src:
    print("ALREADY_PATCHED")
    sys.exit(0)
for old in (OLD_EN, OLD_DIS):
    if src.count(old) != 1:
        print("MISS anchor (%d):\n%s" % (src.count(old), old))
        sys.exit(1)

shutil.copy2(path, path + ".bak-p187")
src = src.replace(OLD_EN, NEW_EN, 1).replace(OLD_DIS, NEW_DIS, 1)
open(path, "w", encoding="utf-8", newline="").write(src)
print("P187_APPLIED backup=%s.bak-p187" % path)
