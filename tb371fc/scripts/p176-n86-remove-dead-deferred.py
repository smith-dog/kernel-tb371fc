#!/usr/bin/env python3
# p176 (TASK-022, kernel n86): remove the dead deferred-work scaffolding.
# Since p174 the notifier calls nvt_ts_resume() synchronously at enable
# start, so nothing schedules nvt_ts_deferred_resume_work anymore (p154
# legacy). Remove: forward declaration, INIT_DELAYED_WORK, the function,
# and the resume_work member. No behavior change.
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
NT = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
NH = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.h")
BK = "/tmp/p176-backup"
os.makedirs(BK, exist_ok=True)
for f in (NT, NH):
    if not os.path.exists(os.path.join(BK, os.path.basename(f) + ".p174")):
        shutil.copy2(f, os.path.join(BK, os.path.basename(f) + ".p174"))
        print("BACKUP %s" % f)

with open(NT, newline="") as f:
    lines = f.read().split("\n")

# 1) forward declaration
di = next(i for i, l in enumerate(lines)
          if l.startswith("static void nvt_ts_deferred_resume_work") and l.rstrip().endswith(";"))
del lines[di]
print("removed fwd decl @", di)

# 2) INIT_DELAYED_WORK
ii = next(i for i, l in enumerate(lines) if "INIT_DELAYED_WORK(&ts->resume_work," in l)
del lines[ii]
print("removed INIT @", ii)

# 3) function definition: start line (no ';') to first closing '}' at col 0
fi = next(i for i, l in enumerate(lines)
          if l.startswith("static void nvt_ts_deferred_resume_work") and not l.rstrip().endswith(";"))
ei = next(i for i in range(fi + 1, len(lines)) if lines[i] == "}")
assert "nvt_drm_panel_notifier_callback" in lines[ei + 1], lines[ei + 1]
del lines[fi:ei + 1]
print("removed function lines", fi + 1, "-", ei + 1)

with open(NT, "w", newline="") as f:
    f.write("\n".join(lines))

with open(NH, newline="") as f:
    hl = f.read().split("\n")
hi = next(i for i, l in enumerate(hl) if "struct delayed_work resume_work;" in l)
del hl[hi]
print("removed header member @", hi)
with open(NH, "w", newline="") as f:
    f.write("\n".join(hl))

# verify nothing references the removed symbols
with open(NT, newline="") as f:
    c = f.read()
for sym in ("deferred_resume", "resume_work"):
    assert sym not in c, "leftover: %s" % sym
print("P176_DONE")
