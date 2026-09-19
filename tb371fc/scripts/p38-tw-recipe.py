#!/usr/bin/env python3
"""p38 — boot-tw 配方忠实复刻: v21 内核 + stock(ZUI ramdisk+stock DTB) + permissive cmdline
用法: python p38-tw-recipe.py <stock_base.img> <kernel.img> <out.img>"""
import struct
import sys

PAGE = 4096
PARTITION_SIZE = 100663296
CMDLINE_OFF = 64
CMDLINE_LEN = 512


def pad(n):
    return (n + PAGE - 1) // PAGE * PAGE


def main():
    stock_path, kernel_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    stock = open(stock_path, "rb").read()
    kernel = open(kernel_path, "rb").read()
    assert stock[:8] == b"ANDROID!"

    cmdline = stock[CMDLINE_OFF:CMDLINE_OFF + CMDLINE_LEN].split(b"\x00")[0]
    print("stock cmdline:", cmdline[:120])
    add = b" androidboot.selinux=permissive"
    assert len(cmdline) + len(add) + 1 <= CMDLINE_LEN, "cmdline no room"
    new_cmdline = cmdline + add + b"\x00"
    new_cmdline = new_cmdline.ljust(CMDLINE_LEN, b"\x00")

    ks = struct.unpack("<I", stock[8:12])[0]
    rs = struct.unpack("<I", stock[16:20])[0]
    ss = struct.unpack("<I", stock[24:28])[0]
    page = struct.unpack("<I", stock[36:40])[0]
    k_off = page
    r_off = k_off + pad(ks)
    s_off = r_off + pad(rs)
    end_described = s_off + pad(ss)

    # 尾部: ramdisk 之后到分区尾(含 DTB 表, 丢 vendor post-DTB 数据, 同 repack 逻辑)
    tail = stock[end_described:]
    FDT = b"\xd0\x0d\xfe\xed"
    last_end = 0
    i = 0
    while True:
        i = tail.find(FDT, i)
        if i < 0:
            break
        total = int.from_bytes(tail[i + 4:i + 8], "big")
        last_end = max(last_end, i + total)
        i += 1
    tail = tail[:last_end + 173] if len(tail) > last_end + 173 else tail
    # 保留原 stub(若在 last_end 后紧随): 与 repack_boot 一致取 +173 stub
    out = bytearray(stock)
    out[CMDLINE_OFF:CMDLINE_OFF + CMDLINE_LEN] = new_cmdline
    # kernel 段替换(长度变化 -> 后段整体重排, 复刻 repack_boot 的重排逻辑)
    ramdisk = stock[r_off:r_off + rs]
    second = stock[s_off:s_off + ss]
    n_ks = len(kernel)
    out = bytearray(stock[:k_off])
    out[8:12] = struct.pack("<I", n_ks)
    out += kernel
    out += b"\x00" * (pad(n_ks) - n_ks)
    out[16:20] = struct.pack("<I", rs)  # ramdisk size 不变
    out += ramdisk
    out += b"\x00" * (pad(rs) - rs)
    out[24:28] = struct.pack("<I", ss)
    out += second
    out += b"\x00" * (pad(ss) - ss)
    # second stage 之后的内容: 重算偏移后重排尾部
    new_s_off = k_off + pad(n_ks) + pad(rs)
    old_tail_start = end_described
    # 头部里没有二级偏移字段需要改(second 偏移由 sizes 推导, 同 repack_boot)
    out += tail
    out += b"\x00" * (PARTITION_SIZE - len(out))
    assert len(out) == PARTITION_SIZE, f"size {len(out)}"
    open(out_path, "wb").write(bytes(out))
    print("wrote", out_path, len(out), "bytes, cmdline tail:", new_cmdline[:80])


if __name__ == "__main__":
    main()
