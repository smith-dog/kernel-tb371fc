#!/usr/bin/env python3
# p146: dedicated wake keyboard device for the nt36532 gesture (TASK-022).
# ZUI input policy (mTpWakeUp=false) drops wake attempts from the touch
# panel device, so report gestures on a plain KEY_WAKEUP/KEY_POWER
# keyboard device instead - treated like the gpio-keys power button.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p146-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p142.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, "r", newline="") as f:
    c = f.read()

# --- 1. gesture report goes to the wake keyboard device ---
old = (
    "\tif (keycode > 0) {\n"
    "\t\tinput_report_key(ts->input_dev, keycode, 1);\n"
    "\t\tinput_sync(ts->input_dev);\n"
    "\t\tinput_report_key(ts->input_dev, keycode, 0);\n"
    "\t\tinput_sync(ts->input_dev);\n"
    "\t}\n"
    "}"
)
new = (
    "\tif (keycode > 0) {\n"
    "\t\t/* TB371FC p146: report on the dedicated wake keyboard device.\n"
    "\t\t * ZUI input policy (mTpWakeUp=false) drops wake attempts from\n"
    "\t\t * the touch panel device, so report like a power button press. */\n"
    "\t\tinput_report_key(ts->wake_input_dev, keycode, 1);\n"
    "\t\tinput_sync(ts->wake_input_dev);\n"
    "\t\tinput_report_key(ts->wake_input_dev, keycode, 0);\n"
    "\t\tinput_sync(ts->wake_input_dev);\n"
    "\t}\n"
    "}"
)
assert c.count(old) == 1, "report body anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK: report path -> wake_input_dev")

# --- 2. probe: allocate + register the wake keyboard device ---
lines = c.split("\n")
idx = None
for i, l in enumerate(lines):
    if l == "\tret = input_register_device(ts->input_dev);":
        idx = i
        break
assert idx is not None, "input_register_device line not found"
# the if (ret) { NVT_ERR(...); } block follows: 3 lines
assert "if (ret) {" in lines[idx + 1], lines[idx + 1]
assert "err_input_register_device_failed" in lines[idx + 3], lines[idx + 3]
insert_at = idx + 4  # after the closing "}"
block = [
    "",
    "\t/* TB371FC p146: dedicated wake keyboard device (TASK-022). A plain",
    "\t * KEY_WAKEUP keyboard is treated as wake-capable by the framework",
    "\t * (like gpio-keys), unlike the touch panel device. */",
    "\tts->wake_input_dev = devm_input_allocate_device(&ts->client->dev);",
    "\tif (!ts->wake_input_dev) {",
    '\t\tNVT_ERR("allocate wake input device failed\\n");',
    "\t\tgoto err_input_register_device_failed;",
    "\t}",
    '\tts->wake_input_dev->name = "nvt_wake_key";',
    "\tts->wake_input_dev->id.bustype = BUS_SPI;",
    "\tinput_set_capability(ts->wake_input_dev, EV_KEY, KEY_WAKEUP);",
    "\tinput_set_capability(ts->wake_input_dev, EV_KEY, KEY_POWER);",
    "\tret = input_register_device(ts->wake_input_dev);",
    "\tif (ret) {",
    '\t\tNVT_ERR("register wake input device failed. ret=%d\\n", ret);',
    "\t\tgoto err_input_register_device_failed;",
    "\t}",
]
lines[idx + 4 : idx + 4] = block
c = "\n".join(lines)
print("OK: wake keyboard device registered at probe")

with open(path, "w", newline="") as f:
    f.write(c)

# --- 3. header: add wake_input_dev member ---
hpath = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.h")
hdst = os.path.join(BK, "nt36xxx.h.orig")
if not os.path.exists(hdst):
    shutil.copy2(hpath, hdst)
with open(hpath, "r", newline="") as f:
    h = f.read()
old_h = "	struct input_dev *input_dev;"
new_h = "	struct input_dev *input_dev;\n	struct input_dev *wake_input_dev;"
assert h.count(old_h) == 1, "header anchor: %d" % h.count(old_h)
h = h.replace(old_h, new_h)
with open(hpath, "w", newline="") as f:
    f.write(h)
print("OK: wake_input_dev member added to nvt_ts_data")
print("P146_DONE")
