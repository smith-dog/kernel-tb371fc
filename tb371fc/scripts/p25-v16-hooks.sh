#!/bin/bash
# v16: start_kernel phase hooks for the blackbox (pinpoint ultra-early death).
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p25-v16.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1

echo "=== 1. printk.c: make tb_bb_copy global + self-mapping ==="
python3 - <<'PYEOF'
p = "/home/smith/kernel-van/kernel/printk/printk.c"
s = open(p, encoding="utf-8").read()
assert "TB371FC_BLACKBOX" in s, "blackbox not in printk.c"
old = """static void __iomem *tb_bb_va;
static void tb_bb_copy(void)
{
	size_t len = log_buf_len;
	char *src;
	if (!tb_bb_va) return;"""
new = """static void __iomem *tb_bb_va;
void tb_bb_copy(void)
{
	size_t len = log_buf_len;
	char *src;
	if (!tb_bb_va) {
		tb_bb_va = ioremap_cache(TB_BB_PHYS, TB_BB_TAIL);
		if (!tb_bb_va) tb_bb_va = ioremap(TB_BB_PHYS, TB_BB_TAIL);
	}
	if (!tb_bb_va) return;"""
assert old in s, "tb_bb_copy pattern not found"
s = s.replace(old, new)
open(p, "w", encoding="utf-8").write(s)
print("printk.c patched")
PYEOF

echo "=== 2. main.c: phase hooks ==="
python3 - <<'PYEOF'
p = "/home/smith/kernel-van/init/main.c"
s = open(p, encoding="utf-8").read()
if "tb_bb_copy" in s:
    print("already patched")
else:
    old1 = "\tsetup_arch(&command_line);"
    assert old1 in s
    s = s.replace(old1, old1 + "\n\ttb_bb_copy();", 1)
    old2 = "\tconsole_init();"
    assert old2 in s
    s = s.replace(old2, old2 + "\n\ttb_bb_copy();", 1)
    old3 = "\t/* Do the rest non-__init'ed, we're now alive */\n\trest_init();"
    assert old3 in s, "rest_init anchor missing"
    s = s.replace(old3, "\ttb_bb_copy();\n" + old3, 1)
    # declaration after the includes block: anchor on an existing extern-ish line
    anchor = "static void set_task_stack_end_magic(void);"
    assert anchor in s
    s = s.replace(anchor, anchor + "\nvoid tb_bb_copy(void);", 1)
    open(p, "w", encoding="utf-8").write(s)
    print("main.c patched with 3 hooks + declaration")
PYEOF

echo "=== 3. rebuild Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p25-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p25-make.log | head -6
if [ ! -f arch/arm64/boot/Image ]; then echo BUILD_FAILED; exit 1; fi
cp arch/arm64/boot/Image $OUT/Image-v16
ls -la $OUT/Image-v16

echo "=== 4. repack boot-v16 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v16 $OUT/boot-v16.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v16.img
echo V16_DONE
