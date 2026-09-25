#!/usr/bin/env python3
"""p219 - TASK-034 P3: fix the p214 wake-lock shims to REGISTER the source
with the PM core. The p214 zeroing shim left the wakeup_source unregistered,
so the first __pm_stay_awake crashed in wakeup_source_report_event (NULL
hrtimer/stat internals). Registered lifecycle: init=memset+name+add,
trash=remove. Applies to the t34 source AND the staging copy. Idempotent."""

FILES = [
	"/home/smith/t34/qca-wifi-host-cmn/qdf/linux/src/qdf_lock.c",
	"/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/staging/qca-wifi-host-cmn/qdf/linux/src/qdf_lock.c",
]
OLD_INIT = "\t*ws = (struct wakeup_source){ .name = name };"
NEW_INIT = ("\tmemset(ws, 0, sizeof(*ws));" + chr(10) +
            "\tws->name = name;" + chr(10) +
            "\twakeup_source_add(ws);")
OLD_TRASH = "\t*ws = (struct wakeup_source){};"
NEW_TRASH = ("\twakeup_source_remove(ws);" + chr(10) +
             "\tmemset(ws, 0, sizeof(*ws));")

for P in FILES:
    src = open(P).read()
    c1, c2 = src.count(OLD_INIT), src.count(OLD_TRASH)
    if c1 == 0 and c2 == 0:
        print("p219: %s nothing to fix" % P.split("/")[-4])
        continue
    assert c1 == 1 and c2 == 1, "p219: %s init x%d trash x%d" % (P, c1, c2)
    src = src.replace(OLD_INIT, NEW_INIT, 1).replace(OLD_TRASH, NEW_TRASH, 1)
    open(P, "w").write(src)
    print("p219: %s shim switched to registered lifecycle" % P.split("/")[-4])
print("p219: OK")
