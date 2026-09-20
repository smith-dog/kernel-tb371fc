#!/usr/bin/env python3
# fix-set-active (TASK-022 follow-up, docs): `fastboot flash boot` flashes
# the CURRENT active slot; following it with `set_active a` would switch to
# the OTHER slot and boot the OLD kernel when the device was on slot b.
# Remove set_active from all flash instructions and document the semantics.
import sys

NOTE = "   > 说明：`fastboot flash boot` 刷入的是**当前活动槽**；不确定可先 " \
       "`fastboot getvar current-slot` 查看。刷完切勿 `set_active` 切到另一槽——那会启动旧内核。\n"

# --- README.md (two blocks) ---
P = "/home/smith/android_kernel_lenovo_paladin/README.md"
c = open(P, encoding="utf-8").read()
bad = "   fastboot set_active a\n   fastboot reboot\n"
assert c.count(bad) == 2, "readme blocks: %d" % c.count(bad)
c = c.replace(bad, "   fastboot reboot\n")
# insert the note after the FIRST fixed block (main how-to)
anchor = "   fastboot flash boot boot-v27n86-patched-q706.img\n   fastboot reboot\n"
assert c.count(anchor) == 1, "main block: %d" % c.count(anchor)
c = c.replace(anchor, anchor + NOTE)
open(P, "w", encoding="utf-8").write(c)
print("README_FIXED")

# --- docs/NOTE-build-and-root.md (one block, bash fence) ---
P2 = "/home/smith/android_kernel_lenovo_paladin/docs/NOTE-build-and-root.md"
c = open(P2, encoding="utf-8").read()
bad2 = "   fastboot set_active a\n   fastboot reboot\n"
assert c.count(bad2) == 1, "doc block: %d" % c.count(bad2)
c = c.replace(bad2, "   fastboot reboot\n" + NOTE.replace("   >", "   > "))
open(P2, "w", encoding="utf-8").write(c)
print("DOC_FIXED")
