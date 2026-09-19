#!/bin/bash
# p45c — 修复 printk.c: p45 误截断的 P39 黑匣子块重建(按 p39 原文 + p43 缓存清洗行)
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p45c-repair.log 2>&1
echo "=== P45C START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. 重建 P39 块 (幂等: TB371FC_BB25R) ==="
if ! grep -q "TB371FC_BB25R" kernel/printk/printk.c; then
  python3 - <<'PYEOF'
src = open('kernel/printk/printk.c', encoding='utf-8').read()
a = 'void __flush_dcache_area(void *addr, size_t len); /* TB371FC_BB24_FIX */\n'
assert a in src, 'prototype anchor missing'
block = '''/* TB371FC_BB25R-restore of TB371FC_BLACKBOX (p39 original + p43 cache clean):
 * periodic + panic copy of the kernel log tail into the no-map reserved region
 * at 0x27E000000, so a later APatch boot can recover the log via a physical
 * memory KPM even if this kernel dies very early. */
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
	__flush_dcache_area(tb_bb_va, len);
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
src = src.replace(a, a + '\n' + block, 1)
open('kernel/printk/printk.c', 'w', encoding='utf-8').write(src)
print('p39 block restored')
PYEOF
[ $? -eq 0 ] || { echo RESTORE_FAIL; exit 1; }
fi
echo "--- integrity checks ---"
grep -c "TB_BB_PHYS" kernel/printk/printk.c
grep -c "void tb_bb_copy(void)" kernel/printk/printk.c
grep -c "early_initcall(tb_bb_init);" kernel/printk/printk.c
grep -c "tb_bb_mark" kernel/printk/printk.c
grep -c "tb_bb_copy();" init/main.c

echo "=== 2. rebuild ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p45c-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|Error [0-9]" $BASE/logs/p45c-make.log | head -10
  echo BUILD_FAILED; exit 1
fi
cp arch/arm64/boot/Image $BASE/out/Image-v25q706
ls -la $BASE/out/Image-v25q706

echo "=== 3. repack boot-v25 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v25q706 $B/out/boot-v25-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
md5sum $BASE/out/boot-v25-q706.img
echo "=== P45C DONE $(date) ==="
