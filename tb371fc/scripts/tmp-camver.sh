#!/system/bin/sh
getprop | grep -iE 'camx|camera.*version|chi' | head -10
strings /vendor/lib64/hw/camera.qcom.so 2>/dev/null | grep -E 'camx.*[0-9]{2}_[0-9]{2}|LA\.UM|CAMX_VERSION' | head -10
strings /vendor/lib64/hw/camera.qcom.so 2>/dev/null | grep -E '^[0-9]{2}-[0-9]{2}-[0-9]{4}|20[0-9]{2}.*[0-9]{2}:[0-9]{2}' | head -5
