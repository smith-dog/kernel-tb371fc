#!/usr/bin/env python3
# p31-kallsyms-parse.py v5 — arm64 4.19 Image kallsyms 提取（v19 实证校准版）
# 实证布局(v19): offsets(s32×num, 首项=0, 严格单调) ... num(0x100对齐u32) ...
#                names(0x100对齐+skew, 首记录=T _text) ... markers(u64×ceil(num/256))
#                ... tokens(256串) ... token_index(u16×256, 0x100对齐内)
# num 存储值可能 = 真值+1（v19: 存储123204, 真值123203）→ 双候选校验。
# rel_base 文件内为 0（boot 重定位补写）→ 地址重建用 text_va 参数。
import struct
import sys

ALIGN = 0x100


def next100(x):
    return (x + ALIGN - 1) & ~(ALIGN - 1)


def walk(data, ns, count):
    p = ns
    n = len(data)
    for _ in range(count):
        if p >= n:
            return None
        ln = data[p]
        if ln == 0 or ln > 128:
            return None
        p += 1 + ln
    return p


def parse_tokens_at(data, p):
    tokens = []
    off = p
    n = len(data)
    for _ in range(256):
        if off >= n:
            return None
        end = data.find(b'\x00', off, off + 65)
        if end < 0 or end == off:
            return None
        if not all(0x20 <= c <= 0x7e for c in data[off:end]):
            return None
        tokens.append(data[off:end])
        off = end + 1
    expect = []
    o = p
    for t in tokens:
        expect.append(o - p)
        o += len(t) + 1
    si = off + 1 if (off % 2) else off
    for cand in range(si, si + 0x110, 2):
        if list(struct.unpack_from('<256H', data, cand)) == expect:
            return tokens
    return None


def decode_check(data, ns, tokens, depth=16):
    q = ns
    for _ in range(depth):
        ln = data[q]
        rec = data[q + 1:q + 1 + ln]
        sym = b''.join(tokens[c] for c in rec)
        t = sym[0]
        if not (0x41 <= t <= 0x7a) or not chr(t).isalpha():
            return False
        name = sym[1:]
        if not name or any(not (0x20 <= c <= 0x7e) for c in name):
            return False
        q += 1 + ln
    return True


def main():
    img_path, out_path = sys.argv[1], sys.argv[2]
    text_va = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0xffffff8008080000
    data = open(img_path, 'rb').read()
    n = len(data)
    print('image size:', n)

    found = None
    for f_off in range(0, n - 0x1000, ALIGN):
        base_num = struct.unpack_from('<I', data, f_off)[0]
        if not (10000 < base_num < 400000):
            continue
        for num in (base_num, base_num - 1):
            if not (10000 < num < 400000):
                continue
            mcount = (num + 255) >> 8
            for k in range(-16, 17):
                ns = f_off + ALIGN + k
                names_end = walk(data, ns, num)
                if names_end is None:
                    continue
                mp = next100(names_end)
                if mp + 8 * mcount > n:
                    continue
                vals = struct.unpack_from('<%dQ' % mcount, data, mp)
                if vals[0] != 0 or any(vals[i] > vals[i + 1] for i in range(mcount - 1)):
                    continue
                tp = next100(mp + 8 * mcount)
                tokens = parse_tokens_at(data, tp)
                if not tokens:
                    continue
                if not decode_check(data, ns, tokens):
                    continue
                found = (f_off, num, ns, tokens)
                break
            if found:
                break
        if found:
            break

    if not found:
        print('KALLSYMS NOT FOUND')
        sys.exit(2)
    f_off, num, ns, tokens = found
    print(f'num_syms={num} @num_candidate=0x{f_off:x} names@0x{ns:x}')

    names = []
    p = ns
    for _ in range(num):
        ln = data[p]
        names.append(data[p + 1:p + 1 + ln])
        p += 1 + ln

    # offsets 数组: 窗口内从后向前扫（取最贴近 names 的合法起点，排除零填充假阳性）
    off_arr = None
    lo = max(0, f_off - num * 4 - 0x900)
    hi = f_off - num * 4 + 0x100
    for cand in range(hi - 4, lo, -4):
        if struct.unpack_from('<I', data, cand)[0] != 0:
            continue
        vals = struct.unpack_from('<%di' % num, data, cand)
        if any(vals[i] > vals[i + 1] for i in range(num - 1)):
            continue
        if vals[-1] < 0x1000 or vals[num // 2] < 0x800:
            continue
        off_arr = cand
        break
    print(f'offsets @0x{off_arr:x}' if off_arr else 'offsets NOT FOUND (names-only)')

    out = []
    for i in range(num):
        name = b''.join(tokens[c] for c in names[i])
        symtype = chr(name[0])
        symname = name[1:].decode('ascii', errors='replace')
        if off_arr is not None:
            o = struct.unpack_from('<i', data, off_arr + i * 4)[0]
            addr = (text_va + o) & 0xFFFFFFFFFFFFFFFF
            out.append('%016x %s %s' % (addr, symtype, symname))
        else:
            out.append('0000000000000000 %c %s' % (symtype, symname))
    open(out_path, 'w', newline='\n').write('\n'.join(out) + '\n')
    print('first 5:', out[:5])
    print(f'wrote {len(out)} symbols -> {out_path}')


if __name__ == '__main__':
    main()
