#!/usr/bin/env python3
# p133: default-arm NT36532 double-tap wake (TASK-022).
# Root cause: the Lenovo touchscreen HAL (vendor.lenovo.hardware.touchscreen@1.0)
# never sends the EV_SYN/SYN_CONFIG WAKEUP_ON switch on the self-built kernel
# (verified live: setting toggle and screen-off both produce no driver events),
# so nvt_gesture_flag stayed false and suspend always took the plain-sleep
# (0x11) path. Default the flag to true: suspend now arms IC gesture mode
# (0x13) whenever the hall gate is open; the SYN_CONFIG path stays functional.
# ZUI's double_tap_enable toggle has no kernel-side effect either way (the HAL
# never delivered it here), documented in TASK-022.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
rel = "drivers/input/touchscreen/nt36532/nt36xxx.c"
BK = "/tmp/p133-backup"
os.makedirs(BK, exist_ok=True)
src = os.path.join(KV, rel)
dst = os.path.join(BK, "nt36xxx.c.orig")
if not os.path.exists(dst):
    shutil.copy2(src, dst)
    print("BACKUP -> %s" % dst)

with open(src, "r", newline="") as f:
    c = f.read()

old = "bool nvt_gesture_flag = false;"
new = ("/* TB371FC p133: default-arm double-tap wake. The Lenovo HAL never sends\n"
       " * the SYN_CONFIG WAKEUP_ON switch on the self-built kernel, so the flag\n"
       " * stayed false and suspend took the plain-sleep path; the hall gate at\n"
       " * suspend still applies. See TASK-022. */\n"
       "bool nvt_gesture_flag = true;")
if c.count(old) != 1:
    print("FAIL: expected exactly 1 occurrence of the flag definition")
    sys.exit(1)
c = c.replace(old, new)

with open(src, "w", newline="") as f:
    f.write(c)
print("OK: nvt_gesture_flag default true")
print("P133_DONE")
