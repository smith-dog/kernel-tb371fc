#!/usr/bin/env python3
# p273b — TASK-044 后续适配:lockdep(4.19 需显式 include;LOCK_STATE_NOT_HELD 为新内核枚举)
import sys, io

SRC = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/misc/ntsync.c"

with io.open(SRC, "r", encoding="utf-8") as f:
    s = f.read()

subs = [
    # 加 lockdep.h include(按字母序插在 ktime.h 后)
    ("#include <linux/ktime.h>\n",
     "#include <linux/ktime.h>\n#include <linux/lockdep.h>\n", 1),
    # LOCK_STATE_NOT_HELD 枚举比较 → 4.19 int 语义(非零=held)
    ("#define ntsync_assert_held(obj) \\\n"
     "\tlockdep_assert((lockdep_is_held(&(obj)->lock) != LOCK_STATE_NOT_HELD) || \\\n"
     "\t\t       ((lockdep_is_held(&(obj)->dev->wait_all_lock) != LOCK_STATE_NOT_HELD) && \\\n"
     "\t\t\t(obj)->dev_locked))\n",
     "#define ntsync_assert_held(obj) \\\n"
     "\tlockdep_assert(lockdep_is_held(&(obj)->lock) || \\\n"
     "\t\t       (lockdep_is_held(&(obj)->dev->wait_all_lock) && \\\n"
     "\t\t\t(obj)->dev_locked))\n", 1),
]

for old, new, expect in subs:
    n = s.count(old)
    if n != expect:
        print("FATAL: pattern x%d (expect %d):\n---\n%s\n---" % (n, expect, old))
        sys.exit(1)
    s = s.replace(old, new)

with io.open(SRC, "w", encoding="utf-8") as f:
    f.write(s)
print("OK: p273b applied (%d subs)" % len(subs))
