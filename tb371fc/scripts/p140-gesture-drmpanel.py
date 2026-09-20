#!/usr/bin/env python3
# p140 (v2 of p134): deliver blank events via the DRM_PANEL notifier chain.
# p134 used the fb chain - wrong channel for two reasons:
#  1. the nt36532 driver registered on the drm_panel chain (CONFIG_DRM_PANEL
#     branch), so p134's fb calls never reached it;
#  2. the fb chain has DORMANT listeners (goodix fp gf_spi, etc.) that had
#     never been called on this platform - waking them at blank crashes the
#     device (every blank == instant reboot on n64/n65/n67/n68).
# This replaces the fb calls with drm_panel_notifier_call_chain() from the
# same two sites, using the dsi panel's own drm_panel object.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
rel = "techpack/display/msm/dsi/dsi_display.c"
BK = "/tmp/p140-backup"
os.makedirs(BK, exist_ok=True)
src = os.path.join(KV, rel)
dst = os.path.join(BK, "dsi_display.c.p134.orig")
if not os.path.exists(dst):
    shutil.copy2(src, dst)
    print("BACKUP -> %s" % dst)

with open(src, "r", newline="") as f:
    c = f.read()

# 1. swap includes: fb/notifier (p134) -> drm_panel
old_inc = "#include <linux/fb.h>\n#include <linux/notifier.h>"
if c.count(old_inc) != 1:
    print("FAIL: p134 include block not found: %d" % c.count(old_inc))
    sys.exit(1)
c = c.replace(old_inc, "#include <drm/drm_panel.h>")

# 2. enable site: fb UNBLANK -> drm_panel EVENT_BLANK/UNBLANK
old_en = """
	/* TB371FC p134: notify fb chain so the touch driver (nt36532)
	 * resumes normal mode on unblank. */
	if (rc == 0) {
		int p134_blank = FB_BLANK_UNBLANK;
		struct fb_event p134_ev = { .data = &p134_blank };
		fb_notifier_call_chain(FB_EVENT_BLANK, &p134_ev);
	}
"""
new_en = """
	/* TB371FC p140: notify drm_panel listeners (nt36532 wake gesture)
	 * on unblank. The fb chain is NOT used - its dormant listeners
	 * (goodix fp) crash on blank (TASK-022). */
	if (rc == 0) {
		int p140_blank = DRM_PANEL_BLANK_UNBLANK;
		struct drm_panel_notifier p140_ev = { .data = &p140_blank };
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EVENT_BLANK, &p140_ev);
	}
"""
if c.count(old_en) != 1:
    print("FAIL: enable block not found: %d" % c.count(old_en))
    sys.exit(1)
c = c.replace(old_en, new_en)
print("OK: enable site -> drm_panel EVENT_BLANK/UNBLANK")

# 3. disable site: fb EARLY POWERDOWN -> drm_panel EARLY_BLANK/POWERDOWN
old_dis = """
	/* TB371FC p134: notify fb chain so the touch driver (nt36532)
	 * suspends into wake-gesture mode on blank (TASK-022). */
	if (rc == 0) {
		int p134_blank = FB_BLANK_POWERDOWN;
		struct fb_event p134_ev = { .data = &p134_blank };
		fb_notifier_call_chain(FB_EARLY_EVENT_BLANK, &p134_ev);
	}
"""
new_dis = """
	/* TB371FC p140: notify drm_panel listeners (nt36532 wake gesture)
	 * before the panel powers down (TASK-022). */
	if (rc == 0) {
		int p140_blank = DRM_PANEL_BLANK_POWERDOWN;
		struct drm_panel_notifier p140_ev = { .data = &p140_blank };
		drm_panel_notifier_call_chain(&display->panel->drm_panel,
			DRM_PANEL_EARLY_EVENT_BLANK, &p140_ev);
	}
"""
if c.count(old_dis) != 1:
    print("FAIL: disable block not found: %d" % c.count(old_dis))
    sys.exit(1)
c = c.replace(old_dis, new_dis)
print("OK: disable site -> drm_panel EARLY_EVENT_BLANK/POWERDOWN")

with open(src, "w", newline="") as f:
    f.write(c)
print("P140_DONE")
