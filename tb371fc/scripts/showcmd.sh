#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
ls techpack/camera/drivers/cam_req_mgr/.*.cmd 2>/dev/null | head -3
f=techpack/camera/drivers/cam_req_mgr/.cam_req_mgr_core.o.cmd
if [ -f "$f" ]; then
  tr ' ' '\n' < "$f" | grep -E '^-I|^-include' | head -25
else
  echo "no cmd file"
fi
