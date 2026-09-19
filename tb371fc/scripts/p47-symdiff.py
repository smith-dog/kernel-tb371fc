#!/usr/bin/env python3
# p47-symdiff.py — 假设(a)检验：掩码立即数掩盖的语义差异
# 升级点（相对 p44 掩码规范化）：
#   1. ADRP+ADD / ADRP+LDR/STR 配对 → 解析成 "符号+偏移" 再比对（暴露引用不同全局变量）
#   2. BL/B/CBZ/TBZ 目标 → 符号名（暴露调用不同函数）
#   3. LDR literal → 字面量池地址 + 池内容值（若为内核地址则符号化）
#   4. difflib 序列对齐（容忍 v26 侧 tb_bb 仪器插入，单独归类为噪声）
#   5. 数据面：对早期窗口引用到的数据符号，比对两镜像中该符号的"内容字节"
# 比对对象：bjsl08（联想，能启动）vs v26（我方，死于 early_fixmap_init 前）
import struct, sys, bisect, difflib
from collections import OrderedDict

BASE = '/mnt/d/work/code-work/project/tb371fc-kernel'
IMGS = {
    'bjsl08': (f'{BASE}/reference/android_device_lenovo_TB371FC-TWRP/prebuilt/kernel',
               f'{BASE}/out/kallsyms-bjsl08.txt'),
    'v26':    (f'{BASE}/out/Image-v26q706',
               '/home/smith/android_kernel_lenovo_paladin/System.map'),
}
TEXT_VA = 0xffffff8008080000

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

class Img:
    def __init__(self, name, img_path, sym_path):
        self.name = name
        self.data = open(img_path, 'rb').read()
        self.syms = load_syms(sym_path)
        self.addrs = [s[0] for s in self.syms]
        self.by_name = {}
        for a, t, n in self.syms:
            self.by_name.setdefault(n, (a, t))
        self.fset = {n for (a, t, n) in self.syms if t in 'tT'}
    def sym_of(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0: return ('?', addr - TEXT_VA)
        a, t, n = self.syms[i]
        return (n, addr - a)
    def fn_bounds(self, name):
        hit = self.by_name.get(name)
        if not hit: return None
        a = hit[0]
        i = bisect.bisect_left(self.addrs, a)
        end = self.addrs[i+1] if i+1 < len(self.addrs) else a + 0x1000
        return (a, end)
    def bytes_at(self, va, n):
        off = va - TEXT_VA
        if off < 0 or off >= len(self.data): return None
        return self.data[off:off+n]

def sext(v, bits):
    m = 1 << (bits - 1)
    return (v ^ m) - m

def tokenize(img, start, end):
    """语义 token 序列：符号化的引用 + 其余指令保持原始字"""
    toks = []
    va = start
    words = []
    while va < end:
        b = img.bytes_at(va, 4)
        if not b: break
        words.append(struct.unpack('<I', b)[0])
        va += 4
    n = len(words)
    used = [False] * n          # 被 ADRP 配对消费的槽
    for i, w in enumerate(words):
        if used[i]:
            toks.append(('PAIR', i))  # 占位，两侧同为 PAIR
            continue
        pc = start + i * 4
        t = None
        # ADRP Xd, imm
        if (w & 0x9F000000) == 0x90000000:
            rd = w & 0x1F
            immlo = (w >> 29) & 3
            immhi = (w >> 5) & 0x7FFFF
            imm = sext((immhi << 2) | immlo, 21) << 12
            base = (pc & ~0xFFF) + imm
            # 向后看 1 条：ADD imm / LDR-STR unsignimm 以 rd 为基
            eff = None; kind = 'ADR'
            if i + 1 < n:
                w2 = words[i+1]
                # ADD imm 64: sf=1 op=0 S=0, [28:24]=10001
                if (w2 & 0xDF000000) == 0x91000000:
                    rn = (w2 >> 5) & 0x1F
                    if rn == rd and (w2 & 0x1F) == rd:
                        sh = (w2 >> 22) & 3
                        imm12 = (w2 >> 10) & 0xFFF
                        eff = base + (imm12 << (12 if sh else 0)); kind = 'ADR+ADD'
                # LDR/STR unsigned imm, Rn==rd
                elif (w2 & 0x3B000000) == 0x39000000:
                    rn = (w2 >> 5) & 0x1F
                    if rn == rd:
                        size = (w2 >> 30) & 3
                        imm12 = (w2 >> 10) & 0xFFF
                        eff = base + (imm12 << size); kind = 'ADR+MEM'
            tgt = eff if eff is not None else base
            name, off = img.sym_of(tgt)
            t = f'{kind}:{name}+0x{off:x}'
            if eff is not None and i + 1 < n:
                used[i+1] = True
        # ADR
        elif (w & 0x9F000000) == 0x10000000:
            immlo = (w >> 29) & 3
            immhi = (w >> 5) & 0x7FFFF
            imm = sext((immhi << 2) | immlo, 21)
            name, off = img.sym_of(pc + imm)
            t = f'ADR:{name}+0x{off:x}'
        # BL / B
        elif (w & 0x7C000000) == 0x14000000:
            imm = sext(w & 0x3FFFFFF, 26) * 4
            tgt = pc + imm
            sub = 'CALL' if (w & 0x80000000) else 'B'
            if sub == 'B' and start <= tgt < end:
                t = f'B:+0x{tgt-start:x}'
            else:
                name, off = img.sym_of(tgt)
                t = f'{sub}:{name}+0x{off:x}'
        # CBZ/CBNZ
        elif (w & 0x7E000000) == 0x34000000:
            imm = sext((w >> 5) & 0x7FFFF, 19) * 4
            tgt = pc + imm
            t = f'B:+0x{tgt-start:x}' if start <= tgt < end else f'CB:{img.sym_of(tgt)[0]}'
        # TBZ/TBNZ
        elif (w & 0x7E000000) == 0x36000000:
            imm = sext((w >> 5) & 0x3FFF, 14) * 4
            tgt = pc + imm
            t = f'B:+0x{tgt-start:x}' if start <= tgt < end else f'TB:{img.sym_of(tgt)[0]}'
        # LDR literal (32/64)
        elif (w & 0x3F000000) == 0x18000000:
            imm = sext((w >> 5) & 0x7FFFF, 19) * 4
            tgt = pc + imm
            name, off = img.sym_of(tgt)
            t = f'LIT@{name}+0x{off:x}'
            lit = img.bytes_at(tgt, 8)
            if lit:
                val = struct.unpack('<Q', lit)[0]
                if TEXT_VA <= val < TEXT_VA + len(img.data):
                    vn, vo = img.sym_of(val)
                    t += f'={vn}+0x{vo:x}'
                elif val != 0 and val < 0x100000:
                    t += f'=imm0x{val:x}'
        if t is None:
            t = f'w{w:08x}'
        toks.append((t, i))
    return [t for t, _ in toks]

def is_noise(tok):
    return 'tb_bb' in tok

def diff_fn(A, B, name, verbose):
    ba, bb = A.fn_bounds(name), B.fn_bounds(name)
    if not ba or not bb:
        print(f'  {name:34s} MISSING in {"bjsl08" if not ba else "v26"}')
        return None
    ta = tokenize(A, *ba)
    tb = tokenize(B, *bb)
    sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
    ops = [o for o in sm.get_opcodes() if o[0] != 'equal']
    # 归类：replace/insert/delete 里全部 token 都是噪声(tb_bb) → 噪声块
    real, noise = [], 0
    for tag, i1, i2, j1, j2 in ops:
        seg_a, seg_b = ta[i1:i2], tb[j1:j2]
        if seg_a and all(is_noise(x) for x in seg_a) and all(is_noise(x) for x in seg_b):
            noise += 1; continue
        if not seg_a and all(is_noise(x) for x in seg_b):
            noise += 1; continue
        if not seg_b and all(is_noise(x) for x in seg_a):
            noise += 1; continue
        real.append((tag, i1, i2, j1, j2))
    if not ops:
        verdict = 'RAW-SEQ-IDENT'
    elif not real:
        verdict = f'SEM-IDENT (noise-blocks x{noise})'
    else:
        verdict = f'SEM-DIFF blocks={len(real)} noise={noise}'
    print(f'  {name:34s} {verdict}')
    if real and verbose:
        for tag, i1, i2, j1, j2 in real[:6]:
            print(f'      [{tag}] bjsl08@{i1}:{ta[i1:i2][:6]}')
            print(f'             v26  @{j1}:{tb[j1:j2][:6]}')
    return (not real)

def data_content_diff(A, B, names):
    """对双方都存在的数据符号，比对内容字节（.bss 越界自动跳过）"""
    print('\n== 数据面：早期函数引用的数据符号内容比对 ==')
    diff, same, skip = [], 0, 0
    for n in sorted(names):
        if n in A.fset or n in B.fset:  # 函数符号不比内容
            continue
        ba, bb = A.fn_bounds(n), B.fn_bounds(n)
        if not ba or not bb: skip += 1; continue
        (sa, ea), (sb, eb) = ba, bb
        da = A.bytes_at(sa, min(ea - sa, 0x4000))
        db = B.bytes_at(sb, min(eb - sb, 0x4000))
        if da is None or db is None: skip += 1; continue
        if len(da) != len(db):
            diff.append((n, f'SIZE {len(da)} vs {len(db)}')); continue
        nd = sum(x == y for x, y in zip(da, db))
        if nd != len(da):
            first = next(i for i in range(len(da)) if da[i] != db[i])
            diff.append((n, f'{nd}/{len(da)} same, first@+0x{first:x}'))
        else:
            same += 1
    print(f'  same={same} diff={len(diff)} skip={skip}')
    for n, why in diff[:80]:
        print(f'  [DIFF] {n:34s} {why}')
    return diff

def collect_refs(img, name, seen):
    """收集函数内 token 引用到的符号（供数据面比对/可达性 BFS 用）"""
    out = set()
    b = img.fn_bounds(name)
    if not b: return out
    for t in tokenize(img, *b):
        for kind in ('ADR', 'CALL', 'LIT@', 'ADR+'):
            pass
        if ':' in t:
            sym = t.split(':', 1)[1].split('+')[0]
            if sym and sym != '?': out.add(sym)
        elif t.startswith('LIT@'):
            sym = t[4:].split('+')[0].split('=')[0]
            if sym and sym != '?': out.add(sym)
    return out

def bfs_functions(A, B, roots, depth):
    """从 roots 出发，沿 CALL/ADR 引用做 BFS，取两镜像都存在的函数名集合"""
    frontier, reach = set(roots), set(roots)
    for _ in range(depth):
        nxt = set()
        for fn in frontier:
            if fn in A.fset:
                nxt |= {s for s in collect_refs(A, fn, None) if s in B.fset and s in A.fset}
        nxt -= reach
        reach |= nxt
        frontier = nxt
        if not frontier: break
    return reach

def diff_fn_full(A, B, name, maxblocks=200):
    """全量块转储：每个 diff 块都打印，带稀疏标签置信度标注"""
    ba, bb = A.fn_bounds(name), B.fn_bounds(name)
    if not ba or not bb:
        print(f'  {name}: MISSING'); return
    ta = tokenize(A, *ba)
    tb = tokenize(B, *bb)
    print(f'== {name}: bjsl08 {ba[1]-ba[0]}B/{len(ta)}tok vs v26 {bb[1]-bb[0]}B/{len(tb)}tok ==')
    sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
    nb = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        nb += 1
        if nb > maxblocks: print('  ... (truncated)'); break
        sa, sb = ta[i1:i2], tb[j1:j2]
        def conf(toks, img):
            out = []
            for t in toks:
                if 'tb_bb' in t: out.append(t + '[BB]'); continue
                # +0x0 结尾=精确命中；off<0x1000=近邻；否则稀疏
                if '+0x0' in t or t.startswith('w') or t.startswith('B:') or t.startswith('PAIR'):
                    out.append(t)
                elif any(t.startswith(p) for p in ('ADR', 'CALL', 'LIT', 'B')):
                    out.append(t + '[sparse?]')
                else:
                    out.append(t)
            return out
        print(f'  #{nb} [{tag}] bjsl08@{i1}({ba[0]+i1*4:#x}):')
        for t in conf(sa, A): print(f'        B {t}')
        print(f'        v26@{j1}({bb[0]+j1*4:#x}):')
        for t in conf(sb, B): print(f'        V {t}')

def main():
    imgs = {k: Img(k, p, s) for k, (p, s) in IMGS.items()}
    A, B = imgs['bjsl08'], imgs['v26']
    print(f'bjsl08: {len(A.syms)} syms, {len(A.data)} B | v26: {len(B.syms)} syms, {len(B.data)} B')

    if len(sys.argv) > 1 and sys.argv[1] == '--dump':
        for n in sys.argv[2:]:
            diff_fn_full(A, B, n)
        return

    # ---- 第一组：p44 已证掩码 IDENT 的 pre-F 函数 + 死亡窗口核心 ----
    watch_preF = ['_head', 'stext', 'el2_setup', 'preserve_boot_args',
                  '__create_page_tables', '__primary_switch', '__primary_switched',
                  '__mmap_switched', '__vet_fdt', '__cpu_setup',
                  'start_kernel', 'set_task_stack_end_magic', 'smp_setup_processor_id',
                  'debug_objects_early_init', 'cgroup_init_early', 'boot_cpu_init',
                  'page_address_init', 'early_security_init', 'lockdep_init',
                  'setup_arch', 'early_fixmap_init', 'early_ioremap_init',
                  'setup_machine_fdt', 'parse_early_param', 'setup_command_line',
                  'boot_cpu_hotplug_init' ]
    print('\n== 组1：死亡窗口函数（语义 token 比对）==')
    clean = []
    for n in watch_preF:
        r = diff_fn(A, B, n, verbose=True)
        if r: clean.append(n)

    # ---- 阳性对照：已知 DIFF 的两个 ----
    print('\n== 组2：阳性对照（p44 已知 norm-DIFF）==')
    for n in ['arm64_memblock_init', 'paging_init']:
        diff_fn(A, B, n, verbose=True)

    # ---- BFS 扩展：把窗口内更多可达函数扫一遍 ----
    print('\n== 组3：BFS 可达集（stext 起 depth4，剔除已比/对照/后段噪声）==')
    reach = bfs_functions(A, B, {'stext', 'start_kernel'}, 4)
    done = set(watch_preF) | {'arm64_memblock_init', 'paging_init'}
    reach = {n for n in reach - done if n in A.fset and n in B.fset
             and not any(x in n for x in ('tb_bb', '__ftrace', 'lockdep', 'kasan', 'ubsan'))}
    print(f'  可达待比函数 {len(reach)} 个')
    for n in sorted(reach):
        diff_fn(A, B, n, verbose=False)

    # ---- 数据面 ----
    refs = set()
    for n in watch_preF:
        refs |= collect_refs(A, n, None)
    data_content_diff(A, B, refs)

if __name__ == '__main__':
    main()
