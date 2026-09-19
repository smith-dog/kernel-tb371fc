#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
make -j1 V=1 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
  techpack/camera/drivers/cam_req_mgr/cam_req_mgr_core.o 2>&1 | tail -40
