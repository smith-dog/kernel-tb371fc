#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
ls -la out/Image-v27n44
ls techpack/camera/drivers/*/*.o 2>/dev/null | wc -l
ls techpack/camera/drivers/cam_req_mgr/cam_req_mgr_core.o techpack/camera/drivers/cam_sensor_module/cam_cci/cam_cci_dev.o 2>/dev/null
aarch64-linux-gnu-nm out/Image-v27n44 2>/dev/null | grep -cE 'cam_req_mgr|cam_cpas|cam_isp' || true
