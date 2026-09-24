#!/usr/bin/env python3
# p186-sysclose-export.py — ksu 32649 (staging) references __arm64_sys_close
# (fd handling around the new su_mount_ns/execve flow); modpost reported it as
# the single undefined symbol for ksu.ko. Extend the p130 shim with the same
# extern+EXPORT_SYMBOL_GPL pattern used for the other syscall wrappers.
P = '/home/smith/android_kernel_lenovo_paladin/drivers/ksu_sym.c'
s = open(P).read()

if '__arm64_sys_close' in s:
    print('p186: already present')
    raise SystemExit(0)

decl = 'extern long __arm64_sys_umount(const struct pt_regs *);\n'
assert s.count(decl) == 1, 'decl anchor not unique'
s = s.replace(decl, decl + 'extern long __arm64_sys_close(const struct pt_regs *);\n', 1)

exp = 'EXPORT_SYMBOL_GPL(__arm64_sys_umount);\n'
assert s.count(exp) == 1, 'export anchor not unique'
s = s.replace(exp, exp + 'EXPORT_SYMBOL_GPL(__arm64_sys_close);\n', 1)

open(P, 'w').write(s)
print('p186: __arm64_sys_close export added')
