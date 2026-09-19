#!/usr/bin/env python3
# p112: add missing cross-dir include to cam_req_mgr/Makefile
# (cam_utils/cam_trace.h does flat #include "cam_req_mgr_core.h"; quoted-include
#  fallback only searches -I paths, and cam_req_mgr's ccflags-y lacked its own dir)
import io

path = "/home/smith/android_kernel_lenovo_paladin/techpack/camera/drivers/cam_req_mgr/Makefile"
with io.open(path, "r", encoding="utf-8") as f:
    src = f.read()

anchor = "ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils\n"
add = "ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr\n"
if "cam_req_mgr/Makefile" and add in src:
    print("already patched")
elif anchor in src and add not in src:
    src = src.replace(anchor, anchor + add, 1)
    with io.open(path, "w", encoding="utf-8") as f:
        f.write(src)
    print("patched cam_req_mgr/Makefile")
else:
    raise SystemExit("anchor not found or unexpected state")

print(open(path).read())
