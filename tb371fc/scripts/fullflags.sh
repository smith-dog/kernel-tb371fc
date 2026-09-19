#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
echo "=== cam_req_mgr/Makefile:"
cat techpack/camera/drivers/cam_req_mgr/Makefile
echo "=== cam_sensor_io/Makefile:"
cat techpack/camera/drivers/cam_sensor_module/cam_sensor_io/Makefile
echo "=== full -I/-include args of cam_req_mgr_core compile:"
make -n ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
  techpack/camera/drivers/cam_req_mgr/cam_req_mgr_core.o 2>/dev/null \
  | grep 'cam_req_mgr_core\.c' | head -1 | tr ' ' '\n' | grep -A1 -E '^-I.*techpack|^-include$'
