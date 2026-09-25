#!/usr/bin/env python3
# p267: kernel-side wlan self-boot trigger — retires the userspace boot_wlan
# poke loop so a clean device (no /data payload, no 98-dlkm hook) gets WiFi
# from the kernel alone. Patch goes to the t34/qc22 SOURCE tree (staging is
# re-copied from here by p224 every build).
import sys

SRC = "/home/smith/t34/qc22/core/hdd/src/wlan_hdd_main.c"
src = open(SRC).read()

if "wlan_boot_self_trigger" in src:
    print("already patched"); sys.exit(0)

# 1) include
A1 = '#include "qdf_delayed_work.h"\n'
N1 = '#include "qdf_delayed_work.h"\n#include <linux/workqueue.h>\n'
assert src.count(A1) == 1
src = src.replace(A1, N1, 1)

# 2) work declaration next to the boot_wlan loader block
A2 = """static struct kobj_attribute wlan_boot_attribute =
	__ATTR(boot_wlan, 0220, NULL, wlan_boot_cb);
"""
N2 = """static struct kobj_attribute wlan_boot_attribute =
	__ATTR(boot_wlan, 0220, NULL, wlan_boot_cb);

/*
 * p267: TB371FC payload-free boot. Kernel-side replacement for the userspace
 * boot_wlan poke loop: retry hdd_driver_load() every 3s (max 200 tries),
 * surviving the cold-boot-calibration guard window, so a clean device needs
 * no /data payload or boot hook to bring wlan up.
 */
static void wlan_boot_self_trigger(struct work_struct *work);
static DECLARE_DELAYED_WORK(wlan_boot_self_work, wlan_boot_self_trigger);
"""
assert src.count(A2) == 1
src = src.replace(A2, N2, 1)

# 3) work body + initial schedule in wlan_init_sysfs()
A3 = """	wlan_loader->loaded_state = 0;
	wlan_loader->attr_group->attrs = attrs;
"""
N3 = """	wlan_loader->loaded_state = 0;
	wlan_loader->attr_group->attrs = attrs;

	schedule_delayed_work(&wlan_boot_self_work, msecs_to_jiffies(10000));
"""
assert src.count(A3) == 1
src = src.replace(A3, N3, 1)

A4 = """static int wlan_init_sysfs(void)
{
	int ret = -ENOMEM;
"""
N4 = """/* p267: see comment at wlan_boot_self_work */
static void wlan_boot_self_trigger(struct work_struct *work)
{
	static int tries;

	if (wlan_loader->loaded_state)
		return;
	if (hdd_driver_load() == 0) {
		wlan_loader->loaded_state = MODULE_INITIALIZED;
		pr_info("p267: wlan self-booted after %d tries", tries);
		return;
	}
	if (++tries >= 200) {
		pr_err("p267: wlan self-boot gave up after %d tries", tries);
		return;
	}
	schedule_delayed_work(&wlan_boot_self_work, msecs_to_jiffies(3000));
}

static int wlan_init_sysfs(void)
{
	int ret = -ENOMEM;
"""
assert src.count(A4) == 1
src = src.replace(A4, N4, 1)

open(SRC, "w").write(src)
print("p267 patched: kernel-side wlan self-boot trigger")
