#!/usr/bin/env python3
# p149: relocate the p146 wake-device block out of the dead error branch.
# p146/p147 inserted the block INSIDE `if (ret) { ... goto err; }` (after the
# goto) - syntactically valid but unreachable, so the compiler dropped it
# (.i had it, .s did not). Move it after the closing brace.
import sys

path = "/home/smith/android_kernel_lenovo_paladin/drivers/input/touchscreen/nt36532/nt36xxx.c"
with open(path, newline="") as f:
    c = f.read()

dead = (
    "\n\t/* TB371FC p146: dedicated wake keyboard device (TASK-022). A plain\n"
    "\t * KEY_WAKEUP keyboard is treated as wake-capable by the framework\n"
    "\t * (like gpio-keys), unlike the touch panel device. */\n"
    "\tts->wake_input_dev = devm_input_allocate_device(&ts->client->dev);\n"
    "\tif (!ts->wake_input_dev) {\n"
    '\t\tNVT_ERR("allocate wake input device failed\\n");\n'
    "\t\tgoto err_input_register_device_failed;\n"
    "\t}\n"
    '\tts->wake_input_dev->name = "nvt_wake_key";\n'
    "\tts->wake_input_dev->id.bustype = BUS_SPI;\n"
    "\tinput_set_capability(ts->wake_input_dev, EV_KEY, KEY_WAKEUP);\n"
    "\tinput_set_capability(ts->wake_input_dev, EV_KEY, KEY_POWER);\n"
    "\tret = input_register_device(ts->wake_input_dev);\n"
    "\tif (ret) {\n"
    '\t\tNVT_ERR("register wake input device failed. ret=%d\\n", ret);\n'
    "\t\tgoto err_input_register_device_failed;\n"
    "\t}\n"
)
n = c.count(dead)
if n != 1:
    print("dead block count = %d (expected 1)" % n)
    sys.exit(1)
c = c.replace(dead, "")
print("removed dead block")

lines = c.split("\n")
idx = None
for i, l in enumerate(lines):
    if l == "\tret = input_register_device(ts->input_dev);":
        idx = i
        break
if idx is None:
    print("register line not found")
    sys.exit(1)
j = idx + 1
if lines[j] != "\tif (ret) {":
    print("unexpected line after register: %r" % lines[j])
    sys.exit(1)
close = None
for k in range(j + 1, j + 8):
    if lines[k] == "\t}":
        close = k
        break
if close is None:
    print("closing brace not found")
    sys.exit(1)
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
lines[close + 1 : close + 1] = block
c = "\n".join(lines)
with open(path, "w", newline="") as f:
    f.write(c)

# verify: the wake block must now FOLLOW the closing brace
pos_reg = c.find("\tret = input_register_device(ts->input_dev);")
seg = c[pos_reg : pos_reg + 700]
ok = ("\t}\n" in seg.split("nvt_wake_key")[0]) and ("goto" not in seg.split("devm_input_allocate")[0].split("if (ret) {")[-1] + "x")
print("--- site preview ---")
print(seg[:450])
print("P149_DONE")
