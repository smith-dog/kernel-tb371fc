#!/usr/bin/env python3
# fix_prinfo.py — printk.c 损坏的 pr_info 字符串修复(跨行字符串 -> 单行 \n)
import re, sys

P = '/home/smith/android_kernel_lenovo_paladin/kernel/printk/printk.c'
src = open(P, encoding='utf-8').read()

pat = re.compile(r'if \(!tb_bb_va\) \{ pr_info\("TB371FC-BB: map failed\n"\); return 0; \}')
hits = pat.findall(src)
print('broken occurrences:', len(hits))
fixed_src, n = pat.subn('if (!tb_bb_va) { pr_info("TB371FC-BB: map failed\\\\n"); return 0; }', src)
print('replaced:', n)
open(P, 'w', encoding='utf-8', newline='\n').write(fixed_src)

# 回读验证
chk = open(P, encoding='utf-8').read()
ok = 'pr_info("TB371FC-BB: map failed\\n");' in chk
print('verified fixed line present:', ok)
print('any remaining newline-in-string:', bool(re.search(r'map failed\n"', chk)))
