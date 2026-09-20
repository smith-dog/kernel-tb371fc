#!/usr/bin/env python3
# p132: remove leftover debug instrumentation and dead blackbox code (TASK-021).
# Removes: V27TRACE x8, BLFIX x2, p100 DCS-read probe block, P108 prints,
#          tb_bb_* blackbox (printk.c tail block, setup.c marks, main.c calls).
# Keeps:   all production fixes (p96/p97/p108 logic/p114/p80/p81/p122/p124/p125).
# Backups: /tmp/p132-backup/
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
BK = "/tmp/p132-backup"
os.makedirs(BK, exist_ok=True)

def load(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()

def save(p, c):
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(c)

def backup(rel):
    src = os.path.join(KV, rel)
    dst = os.path.join(BK, rel.replace("/", "_") + ".orig")
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        print("BACKUP %s -> %s" % (rel, dst))

def remove_line(content, needle, count_expected, label):
    lines = content.split("\n")
    hits = [i for i, l in enumerate(lines) if needle in l]
    if len(hits) != count_expected:
        print("FAIL %s: expected %d hit(s) for %r, got %d" % (label, count_expected, needle, len(hits)))
        sys.exit(1)
    for i in reversed(hits):
        del lines[i]
    print("OK   %s: removed %d line(s) matching %r" % (label, len(hits), needle))
    return "\n".join(lines)

def remove_span(content, start_marker, end_marker, label, to_eof=False):
    si = content.find(start_marker)
    if si < 0:
        print("FAIL %s: start marker not found" % label)
        sys.exit(1)
    if to_eof:
        removed_len = len(content) - si
        content = content[:si]
    else:
        ei = content.find(end_marker, si)
        if ei < 0:
            print("FAIL %s: end marker not found" % label)
            sys.exit(1)
        ei += len(end_marker)
        removed_len = ei - si
        content = content[:si] + content[ei:]
    print("OK   %s: removed %d chars" % (label, removed_len))
    return content

# ---- 1. dsi_panel.c: V27TRACE x5, BLFIX x2, P108 prints x2, p100 block ----
rel = "techpack/display/msm/dsi/dsi_panel.c"
backup(rel)
path = os.path.join(KV, rel)
c = load(path)
c = remove_span(c,
    "\t{\n\t\tstatic ktime_t p100_last;",
    "p100_rc, p100_cfg.rbuf[0], p100_cfg.rbuf[1]);\n\t\t}\n\t}\n",
    "dsi_panel.c p100 probe block")
# BLFIX print is a two-line statement; remove continuation line too
c = remove_line(c, "panel->mi_cfg.bl_enable, panel->bl_config.type);", 1, "dsi_panel.c BLFIX cont.")
for needle in [
    'pr_info("V27TRACE: dsi_panel_reset enter',
    'pr_info("V27TRACE: dsi_panel_power_on enter',
    'pr_info("V27TRACE: dsi_panel_update_backlight enter',
    'pr_info("V27TRACE: dsi_panel_enable enter',
    'pr_info("V27TRACE: dsi_panel_disable enter',
    'pr_info("BLFIX: panel_set_backlight lvl',
    'pr_info("BLFIX: external case reached, calling ktz',
    'pr_info("P108: keep panel power, skip power_on',
    'pr_info("P108: keep panel power, skip power_off',
]:
    c = remove_line(c, needle, 1, "dsi_panel.c")
save(path, c)

# ---- 2. dsi_display.c: V27TRACE x3 ----
rel = "techpack/display/msm/dsi/dsi_display.c"
backup(rel)
path = os.path.join(KV, rel)
c = load(path)
for needle in [
    'pr_info("V27TRACE: dsi_display_set_backlight enter',
    'pr_info("V27TRACE: dsi_display_enable enter',
    'pr_info("V27TRACE: dsi_display_disable enter',
]:
    c = remove_line(c, needle, 1, "dsi_display.c")
save(path, c)

# ---- 3. printk.c: blackbox block to EOF ----
rel = "kernel/printk/printk.c"
backup(rel)
path = os.path.join(KV, rel)
c = load(path)
if not c.rstrip("\n").endswith("}"):
    print("FAIL printk.c: file does not end with tb_bb_mark body as expected")
    sys.exit(1)
c = remove_span(c,
    "/* TB371FC_BLACKBOX: periodic + panic copy of the kernel log tail into the",
    None,
    "printk.c blackbox block",
    to_eof=True)
if not c.endswith("\n"):
    c += "\n"
save(path, c)

# ---- 4. setup.c: extern + 5 tb_bb_mark calls ----
rel = "arch/arm64/kernel/setup.c"
backup(rel)
path = os.path.join(KV, rel)
c = load(path)
c = remove_line(c, "tb_bb_mark", 6, "setup.c")
save(path, c)

# ---- 5. init/main.c: extern + 3 tb_bb_copy calls ----
rel = "init/main.c"
backup(rel)
path = os.path.join(KV, rel)
c = load(path)
c = remove_line(c, "tb_bb_copy", 4, "init/main.c")
save(path, c)

# ---- 6. final tree-wide verification ----
tokens = ["V27TRACE", "BLFIX", "p100_", "P100:", "P108:", "tb_bb_"]
bad = []
for root, dirs, files in os.walk(KV):
    dirs[:] = [d for d in dirs if d not in (".git", "out")]
    for fn in files:
        if not fn.endswith((".c", ".h")):
            continue
        p = os.path.join(root, fn)
        try:
            with open(p, "r", encoding="utf-8", errors="ignore") as f:
                t = f.read()
        except OSError:
            continue
        for tok in tokens:
            if tok in t:
                bad.append((p, tok))
if bad:
    print("FAIL verification, remnants:")
    for p, tok in bad:
        print("  %s: %s" % (p, tok))
    sys.exit(1)
print("VERIFY: tree clean of all removed tokens")
print("P132_DONE")
