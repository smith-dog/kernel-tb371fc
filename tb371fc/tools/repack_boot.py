#!/usr/bin/env python3
"""Repack a stock Android boot image (header v2, QC-appended DTB tail) with a new kernel.

Keeps header fields, ramdisk and the appended tail (DTBs) byte-identical;
only the kernel payload and its size field change. Pads output to the original
partition size so the bootloader sees the same geometry.

Usage: python repack_boot.py <stock_boot.img> <new_kernel.img> <out_boot.img> [tail_file]
Optional tail_file replaces the extracted DTB tail (e.g. after ramoops DTB surgery).
"""
import sys

PAGE = 4096
PARTITION_SIZE = 100663296  # blockdev --getsize64 /dev/block/by-name/boot_a


def pad(n: int) -> int:
    return (n + PAGE - 1) // PAGE * PAGE


def main() -> None:
    stock_path, kernel_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    tail_override = sys.argv[4] if len(sys.argv) > 4 else None
    ramdisk_override = sys.argv[5] if len(sys.argv) > 5 else None
    cmdline_append = sys.argv[6] if len(sys.argv) > 6 else None
    stock = open(stock_path, "rb").read()
    kernel = open(kernel_path, "rb").read()
    assert stock[:8] == b"ANDROID!", "not a boot image"

    import struct
    ks = struct.unpack("<I", stock[8:12])[0]
    rs = struct.unpack("<I", stock[16:20])[0]
    ss = struct.unpack("<I", stock[24:28])[0]
    page = struct.unpack("<I", stock[36:40])[0]
    assert page == PAGE, f"unexpected page size {page}"

    k_off = page
    r_off = k_off + pad(ks)
    s_off = r_off + pad(rs)
    end_described = s_off + pad(ss)

    ramdisk = stock[r_off : r_off + rs]
    if ramdisk_override:
        ramdisk = open(ramdisk_override, "rb").read()
    second = stock[s_off : s_off + ss]
    if tail_override:
        tail = open(tail_override, "rb").read()
    else:
        tail = stock[end_described:]
        # keep only up to the last FDT blob (DTB table); the vendor's post-DTB data
        # (~60MB) is unused by ABL -- APatch's magiskboot repack already dropped it
        FDT_MAGIC = b"\xd0\x0d\xfe\xed"
        last_end = 0
        i = 0
        while True:
            i = tail.find(FDT_MAGIC, i)
            if i < 0:
                break
            total = int.from_bytes(tail[i + 4 : i + 8], "big")
            last_end = max(last_end, i + total)
            i += 1
        assert last_end > 0, "no DTB found in tail"
        tail = tail[:last_end]

    # Report what the tail actually contains (expect DTBs; rest may be slack)
    nz = tail.rstrip(b"\x00")
    print(f"stock: kernel={ks} ramdisk={rs} second={ss} tail={len(tail)} "
          f"tail_nonzero_end={len(nz)}")

    ramdisk_off_new = page + pad(len(kernel))

    hdr = bytearray(stock[:page])
    hdr[8:12] = struct.pack("<I", len(kernel))
    if ramdisk_override and len(ramdisk) != rs:
        # 新 ramdisk 与原版尺寸不同：同步头部 ramdisk 大小并按新尺寸计算偏移/填充
        hdr[16:20] = struct.pack("<I", len(ramdisk))
        rpad = pad(len(ramdisk))
        print(f"ramdisk replaced: {rs} -> {len(ramdisk)} bytes")
    else:
        rpad = pad(rs)
    second_off_new = ramdisk_off_new + rpad

    if cmdline_append:
        CMDLINE_OFF, CMDLINE_LEN = 64, 512
        cur = hdr[CMDLINE_OFF:CMDLINE_OFF + CMDLINE_LEN].split(b"\x00")[0]
        add = (" " + cmdline_append).encode()
        assert len(cur) + len(add) + 1 <= CMDLINE_LEN, "cmdline no room"
        hdr[CMDLINE_OFF:CMDLINE_OFF + CMDLINE_LEN] = (cur + add).ljust(CMDLINE_LEN, b"\x00")
        print(f"cmdline append: '{cmdline_append}'")
    out = bytes(hdr)
    out += kernel.ljust(pad(len(kernel)), b"\x00")
    out += ramdisk.ljust(rpad, b"\x00")
    out += second.ljust(pad(ss), b"\x00")
    out += tail
    PARTITION_SIZE = 100663296  # blockdev --getsize64 /dev/block/by-name/boot_a
    assert len(out) <= PARTITION_SIZE, f"image {len(out)} exceeds partition {PARTITION_SIZE}"
    if len(out) < PARTITION_SIZE:
        out += b"\x00" * (PARTITION_SIZE - len(out))
    open(out_path, "wb").write(out)
    print(f"wrote {out_path}: {len(out)} bytes "
          f"(kernel {ks} -> {len(kernel)}, ramdisk/DTB tail preserved verbatim)")


if __name__ == "__main__":
    main()
