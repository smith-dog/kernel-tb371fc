import io
p = '/home/smith/kernel-van/kernel/printk/printk.c'
s = io.open(p, encoding='utf-8').read()
old = 'pr_info("TB371FC-BB: map failed' + chr(10) + '"); return 0; }'
new = 'pr_info("TB371FC-BB: map failed\n"); return 0; }'
assert old in s, 'pattern not found'
s = s.replace(old, new)
io.open(p, 'w', encoding='utf-8').write(s)
print('fixed')
