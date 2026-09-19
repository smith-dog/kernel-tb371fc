import io

p = '/home/smith/kernel-van/kernel/printk/printk.c'
s = io.open(p, encoding='utf-8').read()

# join the broken pr_info string: real newline inside the C string literal
old = 'pr_info("TB371FC-BB: map failed' + chr(10) + '"); return 0; }'
new = 'pr_info("TB371FC-BB: map failed' + chr(92) + 'n"); return 0; }'
assert old in s, 'broken pattern not found'
s = s.replace(old, new)

# sanity: no other real newline inside string literals of the block
blk = s[s.index('TB371FC_BLACKBOX'):]
assert chr(10) + '");' not in blk, 'another broken string remains'

io.open(p, 'w', encoding='utf-8').write(s)
print('fixed ok')
