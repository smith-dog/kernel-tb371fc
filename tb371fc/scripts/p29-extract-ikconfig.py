#!/usr/bin/env python3
# p29-extract-ikconfig.py — 从 arm64 内核 Image 中提取内嵌 IKCONFIG (.config)
# 原理：CONFIG_IKCONFIG=y 的内核在二进制里内嵌 gzip 压缩的 config，
#       由魔数 "IKCFG_ST"（开始）与 "IKCFG_ED"（结束）包裹。
# 用法：python p29-extract-ikconfig.py <kernel_image> [output.txt]
# 校验：对 out/Apatch-kernel.bin 提取结果与设备 /proc/config.gz 导出件
#       stock/config-4.19.157-perf.txt 逐行一致（6104 非空行，零差异）。
import gzip
import sys


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else path + ".config.txt"
    data = open(path, "rb").read()
    start = data.find(b"IKCFG_ST")
    end = data.find(b"IKCFG_ED")
    if start < 0 or end < 0 or end <= start:
        print("NO IKCONFIG: %s" % path)
        sys.exit(2)
    blob = data[start + 8:end]  # 跳过 "IKCFG_ST" 8 字节魔数
    cfg = gzip.decompress(blob).decode("utf-8", errors="replace")
    open(out, "w", encoding="utf-8", newline="\n").write(cfg)
    lines = [l for l in cfg.splitlines() if l.strip()]
    print("%s -> %s (%d non-blank lines)" % (path, out, len(lines)))


if __name__ == "__main__":
    main()
