#!/usr/bin/env python3
"""p81 — mm8013 driver failure-path fixes (run on WSL as root).

1. mm8013_soc: i2c failure returned cached value BUT ret=-EINVAL, which the
   psy get_property tail escalates (-ENODATA) -> smblib eval_chg_termination
   "Couldn't read SOC value" every few seconds -> battery state churn.
2. mm8013_current / mm8013_current_avg: failure branch wrote the cached value
   then unconditionally clobbered *val with (negative error)*1000 -> garbage
   current readings feeding the charge status machine (st=3/4 flapping).
3. mm8013_read_reg: single-shot i2c read; MM8013 NACKs occasionally during
   measurement cycles -> add one retry after ~1.5ms.
"""
import sys

P = "/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/mm8013c06_battery.c"
src = open(P).read()
orig = src
n = 0

def rep(old, new, expect=1):
    global src, n
    if new in src:
        print("already applied, skip")
        n += 1
        return
    cnt = src.count(old)
    assert cnt == expect, f"anchor x{cnt} (want {expect}): {old[:60]!r}"
    src = src.replace(old, new)
    n += 1

# 1. soc: don't propagate error when serving cached value
rep("""            *val = chip->bat_soc;
            ret = -EINVAL;""",
    """            *val = chip->bat_soc;
            ret = 0;""")

# 2. current: don't clobber cache with garbage on i2c failure
rep("""        if (curr > 32767) {
            curr -= 65536;
        }
        *val = curr*1000;""",
    """        if (curr >= 0) {
            if (curr > 32767) {
                curr -= 65536;
            }
            *val = curr*1000;
        }""", expect=2)

# 3. current_avg: same pattern
# (current_avg covered by the expect=2 edit above)

# 4. read_reg: single retry on NACK
rep("""    ret = i2c_smbus_read_word_data(client, reg);

    if (ret < 0)
        dev_err(&client->dev, "%s: err %d\\n", __func__, ret);""",
    """    ret = i2c_smbus_read_word_data(client, reg);
    if (ret < 0) {
        /* MM8013 NACKs occasionally during its measurement cycle;
         * retry once after a short delay to avoid spurious failures. */
        usleep_range(1000, 2000);
        ret = i2c_smbus_read_word_data(client, reg);
    }
    if (ret < 0)
        dev_err(&client->dev, "%s: err %d\\n", __func__, ret);""")

# ensure usleep_range prototype available
if "#include <linux/delay.h>" not in src:
    src = src.replace("#include <linux/i2c.h>",
                      "#include <linux/i2c.h>\n#include <linux/delay.h>", 1)

open(P, "w").write(src)
print(f"applied {n} edits")
