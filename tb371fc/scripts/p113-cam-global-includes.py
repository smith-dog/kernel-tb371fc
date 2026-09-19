#!/usr/bin/env python3
# p113: techpack/camera/Makefile — global includes for all camera driver dirs.
# Top Makefile exports LINUXINCLUDE; appending + exporting here propagates to the
# recursive drivers/* sub-makes. Uses wildcard so every leaf dir is covered,
# including cross-dir flat includes (cam_trace.h -> cam_req_mgr_core.h,
# cam_sensor_i2c.h -> cam_cci_dev.h).
import io

path = "/home/smith/android_kernel_lenovo_paladin/techpack/camera/Makefile"
with io.open(path, "r", encoding="utf-8") as f:
    src = f.read()

anchor = "obj-y += drivers/\n"
block = (
    "# p113: export camera-wide include paths to the recursive drivers/* sub-makes\n"
    "CAM_DRIVER_DIRS := $(sort $(dir $(wildcard \\\n"
    "\t\t$(srctree)/techpack/camera/drivers/*/ \\\n"
    "\t\t$(srctree)/techpack/camera/drivers/*/*/)))\n"
    "LINUXINCLUDE += $(addprefix -I,$(CAM_DRIVER_DIRS))\n"
    "export LINUXINCLUDE\n"
)
if "CAM_DRIVER_DIRS" in src:
    print("already patched")
elif anchor in src:
    src = src.replace(anchor, block + anchor, 1)
    with io.open(path, "w", encoding="utf-8") as f:
        f.write(src)
    print("patched techpack/camera/Makefile")
else:
    raise SystemExit("anchor not found")

print(src)
