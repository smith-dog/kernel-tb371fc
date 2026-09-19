#!/bin/bash
# p45 — v25-q706: 标记仪器改用 set_fixmap(FIX_TEXT_POKE0) 直连映射
# 动机: v24 的 F 标记在 early_ioremap_init() 之前一行调用 early_memremap——若该
#   路径此时静默失败, "无 V24 魔数"是仪器假阴性。set_fixmap 只依赖紧邻上一行的
#   early_fixmap_init(), 彻底消除该依赖类。
#   独立指针 tb_mark_va, 不与 tb_bb_copy 的 1MB early_memremap 路径共享(防单页
#   fixmap 溢出)。魔数 'V25'。
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p45-v25-fixmap-mark.log 2>&1
echo "=== P45 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. printk.c: tb_bb_mark -> set_fixmap 版 ==="
if ! grep -q "TB371FC_BB25" kernel/printk/printk.c; then
  python3 - <<'PYEOF'
src = open('kernel/printk/printk.c', encoding='utf-8').read()
# BB24 块由 p43 追加在文件末尾: 从其注释起直到 EOF 就是整个 mark 块
start = src.index('/* TB371FC_BB24')
new = '''/* TB371FC_BB25: fixmap-based early markers. Mapping via set_fixmap depends
 * ONLY on early_fixmap_init() (unlike early_memremap, which needs the early
 * ioremap bookkeeping). Markers are dcache-cleaned -> survive warm reset. */
#include <asm/fixmap.h>
static char *tb_mark_va;
static int tb_bb_seq;
void tb_bb_mark(char c)
{
	if (!tb_mark_va) {
		set_fixmap(FIX_TEXT_POKE0, TB_BB_PHYS, PAGE_KERNEL);
		tb_mark_va = (char *)__fix_to_virt(FIX_TEXT_POKE0);
		tb_mark_va[0x30] = 'V';
		tb_mark_va[0x31] = '2';
		tb_mark_va[0x32] = '5';
	}
	if (tb_bb_seq < 0x3F) {
		tb_mark_va[0x40 + tb_bb_seq] = c;
		tb_bb_seq++;
		__flush_dcache_area(tb_mark_va + 0x30, 0x50);
	}
}
'''
src = src[:start] + new
open('kernel/printk/printk.c', 'w', encoding='utf-8').write(src)
print('mark fn replaced with fixmap version')
PYEOF
[ $? -eq 0 ] || { echo PATCH_FAIL; exit 1; }
fi
grep -c "TB371FC_BB25" kernel/printk/printk.c

echo "=== 2. rebuild (增量) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p45-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|Error [0-9]" $BASE/logs/p45-make.log | head -10
  echo BUILD_FAILED; exit 1
fi
cp arch/arm64/boot/Image $BASE/out/Image-v25q706
ls -la $BASE/out/Image-v25q706
grep -c tb_bb_mark System.map

echo "=== 3. repack boot-v25 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v25q706 $BASE/out/boot-v25-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
ls -la $BASE/out/boot-v25-q706.img
md5sum $BASE/out/boot-v25-q706.img
echo "=== P45 DONE $(date) ==="
