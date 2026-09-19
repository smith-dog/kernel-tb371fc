#!/usr/bin/env python3
# p53 — DTB 手术: 给 3 个 DTB 的 /reserved-memory/ramoops@27e000000 节点
#      插入 compatible="ramoops"（FDT_PROP 原位插入 20 字节）
# 原理: 4.19 of_platform.c reserved_mem_matches 会为该节点建 platform device
#      → ram.c probe → console-ramoops 持续记录 dmesg → 任何死法都留痕。
# 用法: python p53-dtbfix.py <in_tail.bin> <out_tail.bin>
import struct, sys

FDT_MAGIC = 0xD00DFEED
BE = lambda b, o, n: int.from_bytes(b[o:o+n], 'big')

def align4(x): return (x + 3) & ~3

def patch_dtb(d):
    """返回补丁后的单个 DTB bytes"""
    assert BE(d, 0, 4) == FDT_MAGIC
    totalsize = BE(d, 4, 4)
    off_struct = BE(d, 8, 4)
    off_strings = BE(d, 12, 4)
    size_strings = BE(d, 32, 4)
    size_struct = BE(d, 36, 4)
    body = d[:totalsize]

    # strings 表里找 "compatible\0" 的偏移
    sblk = body[off_strings:off_strings + size_strings]
    idx = sblk.find(b'compatible\x00')
    assert idx >= 0, 'compatible not in strings table'
    nameoff = idx

    # 定位 ramoops 节点的 reg 属性 token（reg 值含 0x27E000000 大端 u64）
    big = (0x27E000000).to_bytes(8, 'big')
    hit = body.find(big)
    assert hit >= 0, 'ramoops reg value not found'
    prop_tok = hit - 12          # FDT_PROP(4B) len(4B) nameoff(4B) 在 value 前
    assert BE(body, prop_tok, 4) == 3, f'not a FDT_PROP at {prop_tok:#x}'
    assert BE(body, prop_tok + 4, 4) == 16, 'reg len != 16'

    # 插入 FDT_PROP: token=3, len=8, nameoff, value="ramoops\0"
    rec = struct.pack('>III', 3, 8, nameoff) + b'ramoops\x00'
    assert len(rec) == 20
    # 确认插入点在 struct 块内（ramoops 节点内）
    assert off_struct < prop_tok < off_struct + size_struct

    out = bytearray(body[:prop_tok]) + rec + body[prop_tok:]
    # 头部修正: strings 块整体后移 20B; struct 块长度 +20 (v2 修复: 漏改导致 FDT 畸形)
    struct.pack_into('>I', out, 4, totalsize + 20)       # totalsize
    struct.pack_into('>I', out, 12, off_strings + 20)    # off_dt_strings
    struct.pack_into('>I', out, 36, size_struct + 20)    # size_dt_struct
    return bytes(out)

def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, 'rb').read()
    outs, pos, patched = [], 0, 0
    while pos < len(data):
        if BE(data, pos, 4) == FDT_MAGIC:
            ts = BE(data, pos + 4, 4)
            dtb = data[pos:pos + ts]
            outs.append(patch_dtb(dtb))
            patched += 1
            pos += ts
        else:
            # DTB 间填充（若有）
            nxt = data.find(struct.pack('>I', FDT_MAGIC), pos)
            if nxt < 0:
                outs.append(data[pos:])
                break
            outs.append(data[pos:nxt])
            pos = nxt
    open(dst, 'wb').write(b''.join(outs))
    print(f'patched {patched} DTBs -> {dst} ({sum(len(o) for o in outs)} bytes)')

if __name__ == '__main__':
    main()
