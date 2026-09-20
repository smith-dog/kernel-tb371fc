#!/usr/bin/env python3
# p167c (TASK-022): explicit verdict log for the wake recovery, placed in
# the EXECUTING path. p167 edit C logged inside nvt_ts_resume, which has
# been dead code since p154 (UNBLANK only schedules the deferred work).
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p167-backup/nt36xxx.c.p167b.orig"
if not os.path.exists(BK):
    shutil.copy2(path, BK)
    print("BACKUP -> %s" % BK)

with open(path, newline="") as f:
    c = f.read()

old = ('\tNVT_LOG("deferred resume start\\n");\n'
       "\tnvt_bootloader_reset();\n"
       "\tnvt_check_fw_reset_state(RESET_STATE_REK);\n"
       "\tmutex_lock(&ts->lock);")
new = ('\tNVT_LOG("deferred resume start\\n");\n'
       "\tnvt_bootloader_reset();\n"
       "\tif (nvt_check_fw_reset_state(RESET_STATE_REK))\n"
       '\t\tNVT_ERR("TB371FC: deferred fw state check FAILED (wake recovery failed)\\n");\n'
       "\tmutex_lock(&ts->lock);")
assert c.count(old) == 1, "anchor: %d" % c.count(old)
c = c.replace(old, new)

# and drop the dead-code edit C inside nvt_ts_resume (revert to plain call
# so the file does not carry a log line in an uncalled function)
old2 = ('\t} else {\n'
        "\t\tif (nvt_check_fw_reset_state(RESET_STATE_REK))\n"
        '\t\t\tNVT_ERR("TB371FC p167: fw reset state check FAILED after update\\n");\n'
        "\t}")
new2 = ("\t} else {\n"
        "\t\tnvt_check_fw_reset_state(RESET_STATE_REK);\n"
        "\t}")
assert c.count(old2) == 1, "anchor2: %d" % c.count(old2)
c = c.replace(old2, new2)

with open(path, "w", newline="") as f:
    f.write(c)
print("P167C_DONE")
