#!/usr/bin/env python3
"""p214 - TASK-034 P3: shim CAF wakeup_source_init/trash for the 2019-era
qca-wifi-host-cmn. Our 4.19.198 pm_wakeup.h only exposes mainline
create/destroy/drop/add/remove; cmn qdf_lock.c (wakeup_source embedded by
value in qdf_wake_lock_t) needs the CAF in-place init/trash pair. Shims
preserve in-place semantics using compound-literal reset + drop. Idempotent."""

P = "/home/smith/t34/qca-wifi-host-cmn/qdf/linux/src/qdf_lock.c"
NL = chr(10)
OLD = ("#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))" + NL +
       "QDF_STATUS qdf_wake_lock_create(qdf_wake_lock_t *lock, const char *name)")
NEW = ("/* p214: CAF wakeup_source_init/trash shims (kernel lacks them; keep" + NL +
       " * in-place semantics of the by-value qdf_wake_lock_t) */" + NL +
       "static inline void wakeup_source_init(struct wakeup_source *ws, const char *name)" + NL +
       "{" + NL +
       "\t*ws = (struct wakeup_source){ .name = name };" + NL +
       "}" + NL +
       "static inline void wakeup_source_trash(struct wakeup_source *ws)" + NL +
       "{" + NL +
       "\t*ws = (struct wakeup_source){};" + NL +
       "}" + NL +
       NL + OLD)
src = open(P).read()
if "/* p214" in src:
    print("p214: qdf_lock.c ALREADY patched")
else:
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p214: wakeup_source_init/trash shims inserted")
print("p214: OK")
