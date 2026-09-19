#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
make -n ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
  techpack/camera/drivers/cam_req_mgr/cam_req_mgr_core.o 2>/dev/null \
  | grep -oE 'clang[^ ]* .*cam_req_mgr_core\.c' | head -1 > /tmp/clangline.txt
# print only the flag tokens containing -I or -include
tr ' ' '\n' < /tmp/clangline.txt | grep -E '^-I|^-include|^-' | grep -vE '^-W|-O2|-m|-f|-g|^-std|^-nostd|-D__|^-D' | head -30
