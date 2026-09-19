#!/bin/bash
cd /home/smith/android_kernel_lenovo_paladin
aarch64-linux-gnu-nm vmlinux 2>/dev/null | grep -cE ' [tTdDbB] cam_req_mgr' || true
aarch64-linux-gnu-nm vmlinux 2>/dev/null | grep -E ' [tT] (cam_req_mgr_open|cam_cpas_client_register|cam_isp_hw_mgr|cam_smmu_ops)' | head -5
