#!/usr/bin/env python3
# p134: deliver fb blank events to the touch driver (TASK-022, part 2).
# Root cause: nt36532 arms double-tap wake from its legacy fb_notifier
# callback (FB_EARLY_EVENT_BLANK/POWERDOWN -> suspend; FB_EVENT_BLANK/
# UNBLANK -> resume), but the CLO vanilla display stack never calls
# fb_notifier_call_chain, so the driver never sees screen off/on and the
# IC never enters gesture mode (p133's default flag alone is not enough).
# Fix: call the fb chain at the dsi_display_enable/disable success paths.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
rel = "techpack/display/msm/dsi/dsi_display.c"
BK = "/tmp/p134-backup"
os.makedirs(BK, exist_ok=True)
src = os.path.join(KV, rel)
dst = os.path.join(BK, "dsi_display.c.orig")
if not os.path.exists(dst):
    shutil.copy2(src, dst)
    print("BACKUP -> %s" % dst)

with open(src, "r", newline="") as f:
    c = f.read()

# 1. includes (fb.h defines fb_event/FB_*_EVENT/fb_notifier_call_chain)
if "#include <linux/fb.h>" not in c:
    anchor = '#include "dsi_display.h"'
    if c.count(anchor) != 1:
        print("FAIL: include anchor not unique")
        sys.exit(1)
    c = c.replace(anchor, anchor + "\n#include <linux/fb.h>\n#include <linux/notifier.h>")
    print("OK: added fb.h/notifier.h includes")

# 2. helper to insert before a function's final "\treturn rc;"
def insert_before_final_return(content, func_sig, snippet, label):
    fi = content.find(func_sig)
    if fi < 0:
        print("FAIL %s: function not found" % label)
        sys.exit(1)
    fend = content.find("\n}\n", fi)
    if fend < 0:
        print("FAIL %s: function end not found" % label)
        sys.exit(1)
    span = content[fi:fend]
    pos = span.rfind("\treturn rc;")
    if pos < 0:
        print("FAIL %s: final return rc not found" % label)
        sys.exit(1)
    abs_pos = fi + pos
    content = content[:abs_pos] + snippet + content[abs_pos:]
    print("OK   %s: notification inserted" % label)
    return content

# dsi_display_enable: on success, panel is on -> unblank
enable_snip = (
    "\n\t/* TB371FC p134: notify fb chain so the touch driver (nt36532)\n"
    "\t * resumes normal mode on unblank. */\n"
    "\tif (rc == 0) {\n"
    "\t\tint p134_blank = FB_BLANK_UNBLANK;\n"
    "\t\tstruct fb_event p134_ev = { .data = &p134_blank };\n"
    "\t\tfb_notifier_call_chain(FB_EVENT_BLANK, &p134_ev);\n"
    "\t}\n"
)
c = insert_before_final_return(c,
    "int dsi_display_enable(struct dsi_display *display)",
    enable_snip, "dsi_display_enable")

# dsi_display_disable: on success, panel is going off -> early blank powerdown
disable_snip = (
    "\n\t/* TB371FC p134: notify fb chain so the touch driver (nt36532)\n"
    "\t * suspends into wake-gesture mode on blank (TASK-022). */\n"
    "\tif (rc == 0) {\n"
    "\t\tint p134_blank = FB_BLANK_POWERDOWN;\n"
    "\t\tstruct fb_event p134_ev = { .data = &p134_blank };\n"
    "\t\tfb_notifier_call_chain(FB_EARLY_EVENT_BLANK, &p134_ev);\n"
    "\t}\n"
)
c = insert_before_final_return(c,
    "int dsi_display_disable(struct dsi_display *display)",
    disable_snip, "dsi_display_disable")

with open(src, "w", newline="") as f:
    f.write(c)
print("P134_DONE")
