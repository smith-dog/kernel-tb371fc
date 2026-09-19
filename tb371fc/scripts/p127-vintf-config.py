#!/usr/bin/env python3
# p127-vintf-config.py
# 弹窗根治（方案A-2）：对齐 FCM 兼容矩阵的两处硬性 config 杀手项。
# matchKernelConfigs 语义（libvintf 源码实证）：缺失+要求n=通过；存在y+要求n=杀。
# - CONFIG_SYSVIPC=y 杀手 → 关闭；IPC_NS 依赖 (SYSVIPC || POSIX_MQUEUE)，
#   开 CONFIG_POSIX_MQUEUE=y 保住 IPC_NS（Droidspaces 依赖不受损）。
# - CONFIG_ANDROID_PARANOID_NETWORK=y 杀手 → 关闭（AID_INET 组检查遗留特性，无副作用）。
import sys, re

CONF = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/.config'

s = open(CONF).read()
changes = []
def set_cfg(name, val):
    global s
    pat_on  = f'CONFIG_{name}=y'
    pat_m   = f'CONFIG_{name}=m'
    pat_off = f'# CONFIG_{name} is not set'
    if val and (pat_on in s):
        return  # already
    if (not val) and pat_off in s:
        return
    if val:
        if pat_off in s:
            s = s.replace(pat_off, pat_on, 1); changes.append(f'{name}: n->y')
        elif pat_m in s:
            s = s.replace(pat_m, pat_on, 1); changes.append(f'{name}: m->y')
        else:
            # append under its section: simple append is fine for olddefconfig
            s = s.replace('CONFIG_SYSVIPC=y', f'CONFIG_SYSVIPC=y\nCONFIG_{name}=y', 1) if name!='SYSVIPC' else s
            changes.append(f'{name}: appended y')
    else:
        if pat_on in s:
            s = s.replace(pat_on, pat_off, 1); changes.append(f'{name}: y->n')

set_cfg('SYSVIPC', False)
set_cfg('POSIX_MQUEUE', True)
set_cfg('POSIX_MQUEUE_SYSCTL', True)
set_cfg('ANDROID_PARANOID_NETWORK', False)
open(CONF, 'w').write(s)
print('p127 changes:', changes or 'none needed')
