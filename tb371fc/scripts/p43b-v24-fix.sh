#!/bin/bash
# p43b — v24 修复: printk.c 原型前置 + setup.c 五插点(此树 setup_machine_fdt/
# early_fixmap_init 均在 setup_arch 内)。幂等可重放。
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p43b-fix.log 2>&1
echo "=== P43B START $(date) ==="
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
KV=/home/smith/android_kernel_lenovo_paladin
SNAP=/home/smith/snapdragon-llvm-10.0.7
cd $KV || { echo NO_TREE; exit 1; }
export PATH=$SNAP/bin:$PATH

echo "=== 1. printk.c: __flush_dcache_area 原型前置 ==="
if ! grep -q "TB371FC_BB24_FIX" kernel/printk/printk.c; then
  python3 - <<'PYEOF'
src = open('kernel/printk/printk.c', encoding='utf-8').read()
a = '#include <linux/io.h>\n#define TB_BB_PHYS 0x27E000000ULL'
assert a in src, 'bb include anchor'
src = src.replace(a,
  '#include <linux/io.h>\n'
  'void __flush_dcache_area(void *addr, size_t len); /* TB371FC_BB24_FIX */\n'
  '#define TB_BB_PHYS 0x27E000000ULL', 1)
open('kernel/printk/printk.c', 'w', encoding='utf-8').write(src)
print('prototype moved before first use')
PYEOF
fi
grep -c "TB371FC_BB24_FIX" kernel/printk/printk.c

echo "=== 2. setup.c: F/M/P/B/G 标记 ==="
if ! grep -q "tb_bb_mark" arch/arm64/kernel/setup.c; then
  python3 - <<'PYEOF'
src = open('arch/arm64/kernel/setup.c', encoding='utf-8').read()
a = 'void __init setup_arch(char **cmdline_p)'
assert a in src, 'setup_arch landmark'
src = src.replace(a, 'extern void tb_bb_mark(char c); /* TB371FC_BB24 */\n' + a, 1)
n = 0
for landmark, c in (('\tearly_fixmap_init();\n', 'F'),
                    ('\tsetup_machine_fdt(__fdt_pointer);\n', 'M'),
                    ('\tparse_early_param();\n', 'P'),
                    ('\tarm64_memblock_init();\n', 'B'),
                    ('\tpaging_init();\n', 'G')):
    assert src.count(landmark) == 1, (landmark, src.count(landmark))
    src = src.replace(landmark, landmark + "\ttb_bb_mark('%s');\n" % c, 1)
    n += 1
open('arch/arm64/kernel/setup.c', 'w', encoding='utf-8').write(src)
print('setup.c hooks inserted:', n)
PYEOF
fi
grep -c "tb_bb_mark" arch/arm64/kernel/setup.c

echo "=== 3. rebuild (增量) ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-android- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error \
     Image > $BASE/logs/p43b-make.log 2>&1
MK=$?
echo "make exit=$MK"
if [ $MK -ne 0 ]; then
  grep -nE "fatal error:|error:|undefined reference|No rule to make target|Error [0-9]" $BASE/logs/p43b-make.log | head -10
  echo BUILD_FAILED
  exit 1
fi
[ -f arch/arm64/boot/Image ] || { echo NO_IMAGE; exit 1; }
cp arch/arm64/boot/Image $BASE/out/Image-v24q706
ls -la $BASE/out/Image-v24q706
echo "--- tb_bb_mark 符号 ---"
grep -E "tb_bb_mark" System.map
echo "--- Image 内魔数字符串旁证('V24'为运行时写入, 此处查标记函数所在) ---"
grep -c tb_bb_mark System.map

echo "=== 4. repack boot-v24 ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img \
    $BASE/out/Image-v24q706 $BASE/out/boot-v24-q706.img $BASE/out/dtbs/tail-new.bin \
  || { echo REPACK_FAIL; exit 1; }
ls -la $BASE/out/boot-v24-q706.img
md5sum $BASE/out/boot-v24-q706.img
echo "=== P43B DONE $(date) ==="
