#!/usr/bin/env python3
# p147 (all-in-one touch wake fix, TASK-022): re-apply the four fixes onto the
# clean git baseline of nt36xxx.c/.h in one atomic pass.
#   p133: nvt_gesture_flag default true (HAL never sends SYN_CONFIG here)
#   p142: restore the commented-out active_panel assignment (drm_panel
#         notifier registration actually happens)
#   p144: gesture_key_array KEY_POWER -> KEY_WAKEUP
#   p146: dedicated wake keyboard device (ZUI mTpWakeUp=false drops wake
#         attempts from the touch panel device; report gestures on a plain
#         KEY_WAKEUP/KEY_POWER keyboard instead, like gpio-keys)
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
C = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
H = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.h")
BK = "/tmp/p147-backup"
os.makedirs(BK, exist_ok=True)
for p in (C, H):
    d = os.path.join(BK, os.path.basename(p) + ".orig")
    if not os.path.exists(d):
        shutil.copy2(p, d)
        print("BACKUP %s -> %s" % (p, d))

with open(C, "r", newline="") as f:
    c = f.read()

# ---- p133: default-arm gesture flag ----
old = "bool nvt_gesture_flag = false;"
new = ("/* TB371FC p133: default-arm double-tap wake. The Lenovo HAL never sends\n"
       " * the SYN_CONFIG WAKEUP_ON switch on the self-built kernel, so the flag\n"
       " * stayed false and suspend took the plain-sleep path. */\n"
       "bool nvt_gesture_flag = true;")
assert c.count(old) == 1, "p133 anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK p133")

# ---- p142: restore active_panel assignment ----
old = """		if (!IS_ERR(panel)) {
			//active_panel = panel;
			return 0;
		}"""
new = """		if (!IS_ERR(panel)) {
			/* TB371FC p142: keep the panel reference so the wake
			 * gesture notifier registers (was commented out). */
			active_panel = panel;
			return 0;
		}"""
assert c.count(old) == 1, "p142 anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK p142")

# ---- p144: KEY_POWER -> KEY_WAKEUP in gesture_key_array ----
start = c.find("const uint16_t gesture_key_array[] = {")
end = c.find("};", start)
assert start > 0 and end > start, "gesture array not found"
block = c[start:end]
n = block.count("KEY_POWER,")
assert n == 13, "p144: expected 13 KEY_POWER, got %d" % n
c = c[:start] + block.replace("KEY_POWER,", "KEY_WAKEUP,") + c[end:]
print("OK p144 (%d keys)" % n)

# ---- p146: gesture report on dedicated wake keyboard device ----
old = """	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}"""
new = """	if (keycode > 0) {
		/* TB371FC p146: report on the dedicated wake keyboard device.
		 * ZUI input policy (mTpWakeUp=false) drops wake attempts from
		 * the touch panel device, so report like a power button. */
		input_report_key(ts->wake_input_dev, keycode, 1);
		input_sync(ts->wake_input_dev);
		input_report_key(ts->wake_input_dev, keycode, 0);
		input_sync(ts->wake_input_dev);
	}
}"""
assert c.count(old) == 1, "p146 report anchor: %d" % c.count(old)
c = c.replace(old, new)
print("OK p146 report path")

# ---- p146: probe registration of the wake keyboard device ----
lines = c.split("\n")
idx = None
for i, l in enumerate(lines):
    if l == "\tret = input_register_device(ts->input_dev);":
        idx = i
        break
assert idx is not None, "probe anchor not found"
assert "if (ret) {" in lines[idx + 1], lines[idx + 1]
assert "err_input_register_device_failed" in lines[idx + 3], lines[idx + 3]
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
print("OK p146 probe registration")

with open(C, "w", newline="") as f:
    f.write(c)

# ---- p146: header member ----
with open(H, "r", newline="") as f:
    h = f.read()
old_h = "	struct input_dev *input_dev;"
new_h = "	struct input_dev *input_dev;\n	struct input_dev *wake_input_dev;"
assert h.count(old_h) == 1, "header anchor: %d" % h.count(old_h)
h = h.replace(old_h, new_h)
with open(H, "w", newline="") as f:
    f.write(h)
print("OK p146 header member")

# ---- final verification ----
with open(C, "r", newline="") as f:
    v = f.read()
checks = {
    "p133": "nvt_gesture_flag = true;",
    "p142": "active_panel = panel;",
    "p144": "KEY_WAKEUP,  //GESTURE_DOUBLE_CLICK",
    "p146a": 'wake_input_dev->name = "nvt_wake_key";',
    "p146b": "input_report_key(ts->wake_input_dev, keycode, 1);",
}
ok = True
for name, needle in checks.items():
    hit = v.count(needle)
    print("VERIFY %-6s %-45r -> %d" % (name, needle[:42], hit))
    ok = ok and hit == 1
with open(H, "r", newline="") as f:
    hv = f.read()
print("VERIFY header wake_input_dev -> %d" % hv.count("wake_input_dev"))
if not ok:
    print("FAIL: some checks did not pass")
    sys.exit(1)
print("P147_ALL_OK")
