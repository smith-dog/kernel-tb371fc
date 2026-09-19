#!/usr/bin/env python3
# p44-early-diff.py — 三方早期段比对: bjsl08(联想,能启动) / stock-fw(联想,能启动)
#                     / v24(paladin树自编,死于 early_fixmap_init 前)
# 输出: 符号级差异 + 关键函数体规范化比对(消除布局/立即数噪声)
import struct, sys

BASE = '/mnt/d/work/code-work/project/tb371fc-kernel'
IMGS = {
    'bjsl08': (f'{BASE}/reference/android_device_lenovo_TB371FC-TWRP/prebuilt/kernel',
               f'{BASE}/out/kallsyms-bjsl08.txt'),
    'stock':  (f'{BASE}/out/kernel-stock-fw.bin',
               f'{BASE}/out/kallsyms-stock-fw.txt'),
    'v24':    (f'{BASE}/out/Image-v24q706',
               None),  # 用 System.map
}
TEXT_VA = 0xffffff8008080000
EARLY_LIMIT = 0x40000   # _text + 0x40000 内算"早期窗"

def load_syms(path):
    syms = []
    for line in open(path, encoding='utf-8', errors='replace'):
        parts = line.split(None, 2)
        if len(parts) < 3: continue
        try: a = int(parts[0], 16)
        except ValueError: continue
        if a == 0: continue
        syms.append((a, parts[1], parts[2].strip()))
    syms.sort()
    return syms

def norm_word(w):
    if (w & 0x9F000000) == 0x90000000: return w & 0x9F00001F  # ADRP
    if (w & 0x9F000000) == 0x10000000: return w & 0x9F00001F  # ADR
    if (w & 0x7C000000) == 0x14000000: return w & 0xFC000000  # B/BL
    if (w & 0x7E000000) == 0x34000000: return w & 0xFF00001F  # CBZ/CBNZ
    if (w & 0x7E000000) == 0x36000000: return w & 0xFFC0001F  # TBZ/TBNZ
    if (w & 0x3F000000) == 0x18000000: return w & 0xFF00001F  # LDR literal
    return w

def norm_seq(data):
    n = len(data) & ~3
    return [norm_word(struct.unpack_from('<I', data, i)[0]) for i in range(0, n, 4)]

def raw_seq(data):
    n = len(data) & ~3
    return [struct.unpack_from('<I', data, i)[0] for i in range(0, n, 4)]

def cmp_body(d1, d2, name, a1, a2):
    same_raw = sum(x == y for x, y in zip(raw_seq(d1), raw_seq(d2)))
    n1, n2 = norm_seq(d1), norm_seq(d2)
    same_n = sum(x == y for x, y in zip(n1, n2))
    L = min(len(n1), len(n2))
    first = next((i for i in range(L) if n1[i] != n2[i]), None)
    size_note = 'SAME-SIZE' if len(d1) == len(d2) else f'SIZE {len(d1)}vs{len(d2)}'
    verdict = 'IDENTICAL' if same_n == L == len(n1) == len(n2) else f'DIFF raw {same_raw}/{L} norm {same_n}/{L}'
    print(f'  {name:32s} {size_note:18s} {verdict}  first_norm_diff@+{hex(first*4) if first is not None else "-"}')
    return first

def main():
    imgs, syms = {}, {}
    for k, (img, ks) in IMGS.items():
        imgs[k] = open(img, 'rb').read()
    syms['v24'] = load_syms('/home/smith/android_kernel_lenovo_paladin/System.map')
    syms['bjsl08'] = load_syms(IMGS['bjsl08'][1])
    syms['stock'] = load_syms(IMGS['stock'][1])

    print('== _text 锚点 ==')
    for k in syms:
        t = [s for s in syms[k] if s[2] == '_text']
        print(f'  {k}: _text={"%016x" % t[0][0] if t else "N/A"}  symbols={len(syms[k])}')

    print('\n== 早窗符号集 diff (VA<_text+0x40000, 名称级) ==')
    def early_names(k):
        return {n for (a, t, n) in syms[k] if a - TEXT_VA < EARLY_LIMIT}
    e8, es, ev = early_names('bjsl08'), early_names('stock'), early_names('v24')
    print(f'  bjsl08={len(e8)} stock={len(es)} v24={len(ev)}')
    only_lenovo = sorted((e8 | es) - ev)
    only_v24 = sorted(ev - (e8 | es))
    print(f'  [联想有,v24无] {len(only_lenovo)}: {only_lenovo[:60]}')
    print(f'  [v24有,联想无] {len(only_v24)}: {only_v24[:60]}')

    print('\n== 关键早期函数体比对 (bjsl08 vs v24) ==')
    watch = ['_head', 'stext', 'preserve_boot_args', '__create_page_tables',
             '__primary_switch', '__primary_switched', 'el2_setup', '__vet_fdt',
             'start_kernel', 'setup_arch', 'early_fixmap_init', 'early_ioremap_init',
             'setup_machine_fdt', 'parse_early_param', 'arm64_memblock_init',
             'paging_init', 'boot_cpu_init', 'smp_setup_processor_id',
             'setup_command_line', 'rest_init', 'cgroup_init_early']
    for name in watch:
        loc = {}
        ok = True
        for k in ('bjsl08', 'v24'):
            hits = [(a, t) for (a, t, n) in syms[k] if n == name]
            if not hits:
                print(f'  {name:32s} MISSING in {k}'); ok = False; break
            loc[k] = hits[0][0]
        if not ok: continue
        bodies = []
        for k in ('bjsl08', 'v24'):
            a = loc[k]
            slist = syms[k]
            i = next(i for i, s in enumerate(slist) if s[0] == a)
            end = slist[i+1][0] if i+1 < len(slist) else a + 0x2000
            off = a - TEXT_VA
            bodies.append(imgs[k][off:off + (end - a)])
        cmp_body(bodies[0], bodies[1], name, loc['bjsl08'], loc['v24'])

    print('\n== 联想双件对照 (bjsl08 vs stock) 头 0x400 原始字节 ==')
    d8, ds = imgs['bjsl08'][0x40:0x440], imgs['stock'][0x40:0x440]
    same = sum(x == y for x, y in zip(raw_seq(d8), raw_seq(ds)))
    print(f'  raw words same {same}/{min(len(d8),len(ds))//4} (of 0x100 instrs)')

    print('\n== 头 0x40..0x440: bjsl08 vs v24 原始比对 ==')
    d8, dv = imgs['bjsl08'][0x40:0x440], imgs['v24'][0x40:0x440]
    same = sum(x == y for x, y in zip(raw_seq(d8), raw_seq(dv)))
    first = next((i for i in range(0x100) if d8[i*4:i*4+4] != dv[i*4:i*4+4]), None)
    print(f'  raw words same {same}/256, first_diff@file+{hex(0x40+first*4) if first is not None else "none"}')

if __name__ == '__main__':
    main()
