#!/system/bin/sh
echo "== boot_completed: $(getprop sys.boot_completed)"
echo "== camera devices:"
ls -la /dev/cam_req_mgr 2>/dev/null
ls /dev/media* /dev/v4l* 2>/dev/null
echo "== provider alive?"
ps -A | grep camera.provider
echo "== provider crashes in last minutes:"
logcat -b crash -d -t 100 2>/dev/null | grep -c 'camera.provider'
echo "== kernel camera probes:"
dmesg | grep -iE 'cam_req_mgr|cam_cci|cam_cpas|cam_isp|camss|spectra' | tail -20
