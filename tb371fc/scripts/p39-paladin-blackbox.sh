#!/bin/bash
# p39 — v22: 移植 v17 黑匣子(日志转储到 0x27E000000) 进 paladin 树 + 重建打包
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p39-blackbox.log 2>&1
echo "=== P39 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
export PATH=/usr/local/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-elf/bin:$PATH
cd $KV || exit 1

echo "=== 1. append blackbox to printk.c (idempotent) ==="
if ! grep -q "TB371FC_BLACKBOX" kernel/printk/printk.c; then
  python3 - <<'PYEOF'
blk = '''

/* TB371FC_BLACKBOX: periodic + panic copy of the kernel log tail into the
 * no-map reserved region at 0x27E000000, so a later APatch boot can recover
 * the log via a physical-memory KPM even if this kernel dies very early. */
#include <linux/io.h>
#define TB_BB_PHYS 0x27E000000ULL
#define TB_BB_TAIL 0x100000UL
extern void *early_memremap(resource_size_t phys_addr, size_t size);
static void *tb_bb_va;
void tb_bb_copy(void)
{
	size_t len = log_buf_len;
	char *src;
	if (!tb_bb_va) {
		tb_bb_va = early_memremap(TB_BB_PHYS, TB_BB_TAIL);
		if (!tb_bb_va) tb_bb_va = ioremap_cache(TB_BB_PHYS, TB_BB_TAIL);
		if (!tb_bb_va) tb_bb_va = ioremap(TB_BB_PHYS, TB_BB_TAIL);
	}
	if (!tb_bb_va) return;
	src = log_buf;
	if (len > TB_BB_TAIL) { src = log_buf + (len - TB_BB_TAIL); len = TB_BB_TAIL; }
	memcpy(tb_bb_va, src, len);
}
static void tb_bb_kmsg_dump(struct kmsg_dumper *d, enum kmsg_dump_reason reason)
{ tb_bb_copy(); }
static struct kmsg_dumper tb_bb_dumper = { .dump = tb_bb_kmsg_dump };
static int tb_bb_thread(void *arg)
{
	while (!kthread_should_stop()) { tb_bb_copy(); ssleep(5); }
	return 0;
}
static int __init tb_bb_init(void)
{
	tb_bb_copy();
	if (!tb_bb_va) { pr_info("TB371FC-BB: map failed\\n"); return 0; }
	if (kmsg_dump_register(&tb_bb_dumper)) return 0;
	kthread_run(tb_bb_thread, NULL, "tb371fc-bb");
	return 0;
}
early_initcall(tb_bb_init);
'''
open('kernel/printk/printk.c', 'a', encoding='utf-8').write(blk)
print('blackbox appended')
PYEOF
fi
grep -c "TB371FC_BLACKBOX" kernel/printk/printk.c

echo "=== 2. phase hooks in init/main.c (idempotent) ==="
if ! grep -q "tb_bb_copy" init/main.c; then
  python3 - <<'PYEOF'
src = open('init/main.c', encoding='utf-8').read()
# extern 声明: start_kernel 定义行之前
a = 'asmlinkage __visible void __init start_kernel(void)'
assert a in src
src = src.replace(a, 'void tb_bb_copy(void);\n' + a, 1)
n = 0
for landmark in ('\tsetup_arch(&command_line);\n', '\tconsole_init();\n', '\t/* Do the rest non-__init\'ed, we\'re now alive */\n'):
    assert landmark in src, landmark
    src = src.replace(landmark, landmark + '\ttb_bb_copy();\n', 1)
    n += 1
open('init/main.c', 'w', encoding='utf-8').write(src)
print('hooks inserted:', n)
PYEOF
fi
grep -n "tb_bb_copy" init/main.c | head -5

echo "=== 3. rebuild (printk/main 变更, 增量) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-none-elf- CC=aarch64-none-elf-gcc KCFLAGS=-Wno-error Image > $BASE/logs/p39-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "error:" $BASE/logs/p39-make.log | head -6
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v22q706
ls -la $BASE/out/Image-v22q706

echo "=== 4. repack (APatch 基板 + tail-new, 与 v19/v20/v21 同基) ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $BASE/out/Image-v22q706 $BASE/out/boot-v22-q706.img $BASE/out/dtbs/tail-new.bin
ls -la $BASE/out/boot-v22-q706.img
md5sum $BASE/out/boot-v22-q706.img
echo "=== P39 DONE $(date) ==="
