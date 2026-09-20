#!/usr/bin/env python3
# p137: flush persistent_ram records to DRAM during oops/panic (TASK-022).
# The ramoops region (ramoops@27e000000, 2MB no-map) is mapped WB via
# memremap; persistent_ram_update writes through the cached mapping, so on
# this platform the dirty cache lines do not reach DRAM before the warm
# reset and every panic record was lost. Clean the dcache for the updated
# range while oops_in_progress so panic/console records survive the reset.
# (Same lesson as the old blackbox p43 cache-clean; mem_type=1/ioremap is
# NOT an alternative here - it faults early boot, see n66 bootloop.)
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
rel = "fs/pstore/ram_core.c"
BK = "/tmp/p137-backup"
os.makedirs(BK, exist_ok=True)
src = os.path.join(KV, rel)
dst = os.path.join(BK, "ram_core.c.orig")
if not os.path.exists(dst):
    shutil.copy2(src, dst)
    print("BACKUP -> %s" % dst)

with open(src, "r", newline="") as f:
    c = f.read()

# 1. include asm/cacheflush.h for __flush_dcache_area
inc_anchor = "#include <linux/pstore_ram.h>"
if c.count(inc_anchor) != 1:
    print("FAIL: include anchor not unique: %d" % c.count(inc_anchor))
    sys.exit(1)
c = c.replace(inc_anchor, inc_anchor + "\n#include <asm/cacheflush.h>")

# 2. dcache clean on panic-path updates
old = ("""	struct persistent_ram_buffer *buffer = prz->buffer;
	memcpy_toio(buffer->data + start, s, count);
	persistent_ram_update_ecc(prz, start, count);
}""")
new = ("""	struct persistent_ram_buffer *buffer = prz->buffer;
	memcpy_toio(buffer->data + start, s, count);
	persistent_ram_update_ecc(prz, start, count);
	/* TB371FC p137: the ramoops region is mapped WB; without an
	 * explicit clean the dirty lines are lost on the warm reset that
	 * follows a panic, so flush while the oops is in progress. */
	if (oops_in_progress)
		__flush_dcache_area(buffer->data + start, count);
		__flush_dcache_area(buffer, sizeof(struct persistent_ram_buffer));
}""")
if c.count(old) != 1:
    print("FAIL: expected exactly 1 occurrence of persistent_ram_update body, got %d" % c.count(old))
    sys.exit(1)
c = c.replace(old, new)

with open(src, "w", newline="") as f:
    f.write(c)
print("OK: persistent_ram_update now dcache-cleans on panic")
print("P137_DONE")
