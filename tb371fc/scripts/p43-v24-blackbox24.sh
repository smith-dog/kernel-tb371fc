#!/bin/bash
# p43 — v24-q706: 黑匣子仪表升级（针对 v23 点火后用户观察"长时间挂住不重启"）
# 动机: v15~v23 黑匣子用 early_memremap(WB 缓存) 写 0x27E000000 但从不 clean dcache
#   —— 若死在 setup_arch 内部, dirty cache 行在暖重启时丢失, "区域空"是假阴性。
#   v24: ① setup_arch 内部 5 个单字节标记(F/M/P/B/G), 每次写后 __flush_dcache_area;
#        ② 原 tb_bb_copy 的 1MB 拷贝后补 dcache clean。
#   标记读取: 区域偏移 0x30='V24' 魔数, 0x40..0x7F=标记序列。
# 运行: wsl -u root -e bash <本文件>   (前置: p42 已构建过 v23 的树)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p43-v24-blackbox24.log 2>&1
echo "=== P43 START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. printk.c: tb_bb_mark + cache clean (幂等) ==="
if ! grep -q "TB371FC_BB24" kernel/printk/printk.c; then
  python3 - <<'PYEOF'
src = open('kernel/printk/printk.c', encoding='utf-8').read()
a = 'memcpy(tb_bb_va, src, len);'
assert a in src, 'tb_bb_copy memcpy landmark'
src = src.replace(a, a + '\n\t__flush_dcache_area(tb_bb_va, len);', 1)
mark = '''

/* TB371FC_BB24: early-phase markers. Each mark is one byte, dcache-cleaned
 * immediately so it survives warm reset even if the kernel hangs later. */
#include <asm/cacheflush.h>
static int tb_bb_seq;
void tb_bb_mark(char c)
{
	if (!tb_bb_va) {
		tb_bb_va = early_memremap(TB_BB_PHYS, TB_BB_TAIL);
		if (!tb_bb_va) return;
		((volatile char *)tb_bb_va)[0x30] = 'V';
		((volatile char *)tb_bb_va)[0x31] = '2';
		((volatile char *)tb_bb_va)[0x32] = '4';
	}
	if (tb_bb_seq < 0x3F) {
		((volatile char *)tb_bb_va)[0x40 + tb_bb_seq] = c;
		tb_bb_seq++;
		__flush_dcache_area(tb_bb_va + 0x30, 0x50);
	}
}
'''
src += mark
open('kernel/printk/printk.c', 'w', encoding='utf-8').write(src)
print('printk.c patched')
PYEOF
fi
grep -c "TB371FC_BB24" kernel/printk/printk.c

echo "=== 2. setup.c: M/P/B/G 标记 (幂等) ==="
if ! grep -q "tb_bb_mark" arch/arm64/kernel/setup.c; then
  python3 - <<'PYEOF'
src = open('arch/arm64/kernel/setup.c', encoding='utf-8').read()
a = 'void __init setup_arch(char **cmdline_p)'
assert a in src, 'setup_arch landmark'
src = src.replace(a, 'extern void tb_bb_mark(char c);\n' + a, 1)
n = 0
for landmark, c in (('\tmdesc = setup_machine_fdt(__fdt_pointer);\n', 'M'),
                    ('\tparse_early_param();\n', 'P'),
                    ('\tarm64_memblock_init();\n', 'B'),
                    ('\tpaging_init();\n', 'G')):
    assert landmark in src, landmark
    src = src.replace(landmark, landmark + "\ttb_bb_mark('%s');\n" % c, 1)
    n += 1
open('arch/arm64/kernel/setup.c', 'w', encoding='utf-8').write(src)
print('setup.c hooks inserted:', n)
PYEOF
fi
grep -c "tb_bb_mark" arch/arm64/kernel/setup.c

echo "=== 3. fdt.c: F 标记 (early_fixmap_init 后, 幂等) ==="
if ! grep -q "tb_bb_mark" drivers/of/fdt.c; then
  python3 - <<'PYEOF'
src = open('drivers/of/fdt.c', encoding='utf-8').read()
a = '\tearly_fixmap_init();'
assert src.count(a) == 1, 'early_fixmap_init count=%d' % src.count(a)
src = src.replace(a, a + "\n\textern void tb_bb_mark(char c); tb_bb_mark('F');", 1)
open('drivers/of/fdt.c', 'w', encoding='utf-8').write(src)
print('fdt.c patched')
PYEOF
fi
grep -c "tb_bb_mark" drivers/of/fdt.c

echo "=== 4. rebuild (增量: printk/setup/fdt + 重链接) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p43-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|No rule to make target|Error [0-9]" $BASE/logs/p43-make.log | head -10
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v24q706
ls -la $BASE/out/Image-v24q706
echo "--- tb_bb_mark in v24 System.map ---"
grep -E "tb_bb_mark" System.map
echo "--- v23→v24 差异文件确认(仅仪表) ---"
grep -c "TB371FC_BB24" kernel/printk/printk.c

echo "=== 5. repack boot-v24 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v24q706 $BASE/out/boot-v24-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
ls -la $BASE/out/boot-v24-q706.img
md5sum $BASE/out/boot-v24-q706.img
echo "=== P43 DONE $(date) ==="
