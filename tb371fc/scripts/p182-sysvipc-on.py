#!/usr/bin/env python3
# p182-sysvipc-on.py — Steam ARM64 requires SysV semaphores (semget) and shared
# memory (shmget); POSIX sem_open is not enough for its tier0 threadtools.
# Revert the p127 VINTF alignment on this one symbol: CONFIG_SYSVIPC=y.
# Consequence (known): the FCM target-level=5 matrix declares SYSVIPC=n, and
# libvintf's matchKernelConfigs kills "present y + required n" ->
# Build.isBuildConsistent() fails -> ZUI shows the "设备内部出现问题" popup once
# per boot. Cosmetic, non-blocking. IPC_NS/POSIX_MQUEUE stay as they are.
import sys

CONF = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/.config'

s = open(CONF).read()
if 'CONFIG_SYSVIPC=y' in s:
    print('p182: already y')
    sys.exit(0)
assert '# CONFIG_SYSVIPC is not set' in s, 'SYSVIPC baseline not found'
s = s.replace('# CONFIG_SYSVIPC is not set', 'CONFIG_SYSVIPC=y', 1)
open(CONF, 'w').write(s)
print('p182: SYSVIPC n->y written')
