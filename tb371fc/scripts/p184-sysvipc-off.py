#!/usr/bin/env python3
# p184-sysvipc-off.py — revert p182: CONFIG_SYSVIPC=y -> not set.
# Reason: the VINTF FCM (target-level 5) declares SYSVIPC=n; with y,
# libvintf fails Build.isBuildConsistent() -> the "设备内部出现问题" boot
# popup returns (TASK-010 追加35). User decided to drop Steam support and
# restore the popup-free state. IPC_NS stays enabled via POSIX_MQUEUE=y
# (p127 arrangement, unchanged).
import sys

CONF = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/.config'

s = open(CONF).read()
if '# CONFIG_SYSVIPC is not set' in s:
    print('p184: already off')
    sys.exit(0)
assert 'CONFIG_SYSVIPC=y' in s, 'SYSVIPC=y baseline not found'
s = s.replace('CONFIG_SYSVIPC=y', '# CONFIG_SYSVIPC is not set', 1)
open(CONF, 'w').write(s)
print('p184: SYSVIPC y->n written')
