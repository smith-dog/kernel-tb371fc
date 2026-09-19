#!/bin/bash
# v15: vanilla + Docker config set + EARLY BLACKBOX log dumper.
# Blackbox: every 5s + on panic/oops, copy kernel log tail to the no-map
# reserved region at 0x27E000000, so the APatch dump KPM can recover it
# from any later boot - even when the kernel dies before ramoops probes.
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p24-blackbox.log 2>&1
echo START
KV=/home/smith/kernel-van
BASE=/mnt/d/work/code-work/project/tb371fc-kernel
OUT=$BASE/out
export PATH=/usr/lib/llvm-17/bin:/usr/local/arm-as:$PATH

cd $KV || exit 1

echo "=== 1. Docker + namespace config set ==="
./scripts/config \
  -e NAMESPACES -e PID_NS -e IPC_NS -e UTS_NS -e NET_NS -e USER_NS -e CGROUP_NS \
  -e CGROUPS -e CGROUP_CPUACCT -e CGROUP_DEVICE -e CGROUP_FREEZER -e CGROUP_SCHED \
  -e CPUSETS -e MEMCG -e MEMCG_SWAP -e CGROUP_PIDS -e BLK_CGROUP -e BLK_DEV_THROTTLING \
  -e KEYS -e POSIX_MQUEUE -e OVERLAY_FS \
  -e VETH -e BRIDGE -e BRIDGE_NETFILTER -e NETFILTER -e NETFILTER_ADVANCED \
  -e NF_CONNTRACK -e NETFILTER_XT_MATCH_ADDRTYPE -e NETFILTER_XT_MATCH_CONNTRACK \
  -e NETFILTER_XT_MATCH_IPVS -e IP_NF_FILTER -e IP_NF_NAT -e IP_NF_TARGET_MASQUERADE \
  -e NETFILTER_XT_MASQUERADE -e IP_NF_IPTABLES -e TUN -e FUSE_FS
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- olddefconfig
echo "--- config verify ---"
grep -E "^CONFIG_PID_NS|^CONFIG_IPC_NS|^CONFIG_NET_NS|^CONFIG_USER_NS|^CONFIG_MEMCG=|^CONFIG_OVERLAY_FS|^CONFIG_VETH|^CONFIG_BRIDGE=|^CONFIG_POSIX_MQUEUE|^CONFIG_IP_NF_TARGET_MASQUERADE" .config

echo "=== 2. append blackbox dumper to printk.c ==="
if ! grep -q "TB371FC_BLACKBOX" kernel/printk/printk.c; then
cat >> kernel/printk/printk.c <<'EOFC'

/* TB371FC_BLACKBOX: periodic + panic copy of the kernel log tail into the
 * no-map reserved region at 0x27E000000, so a later APatch boot can recover
 * the log via a physical-memory KPM even if this kernel dies very early. */
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#define TB_BB_PHYS 0x27E000000ULL
#define TB_BB_TAIL 0x100000UL
static void __iomem *tb_bb_va;
static void tb_bb_copy(void)
{
	size_t len = log_buf_len;
	char *src;
	if (!tb_bb_va) return;
	src = log_buf;
	if (len > TB_BB_TAIL) { src = log_buf + (len - TB_BB_TAIL); len = TB_BB_TAIL; }
	memcpy((void *)tb_bb_va, src, len);
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
	tb_bb_va = ioremap_cache(TB_BB_PHYS, TB_BB_TAIL);
	if (!tb_bb_va) tb_bb_va = ioremap(TB_BB_PHYS, TB_BB_TAIL);
	if (!tb_bb_va) { pr_info("TB371FC-BB: map failed\n"); return 0; }
	if (kmsg_dump_register(&tb_bb_dumper)) { pr_info("TB371FC-BB: dump reg failed\n"); return 0; }
	kthread_run(tb_bb_thread, NULL, "tb371fc-bb");
	pr_info("TB371FC-BB: armed va=%px\n", tb_bb_va);
	return 0;
}
early_initcall(tb_bb_init);
EOFC
echo "blackbox appended"
else
echo "blackbox already present"
fi

echo "=== 3. rebuild Image ==="
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as KCFLAGS=-Wno-error Image > $BASE/logs/p24-make.log 2>&1
echo "make exit=$?"
grep -nE "error:|undefined reference" $BASE/logs/p24-make.log | head -6
if [ ! -f arch/arm64/boot/Image ]; then echo BUILD_FAILED; exit 1; fi
cp arch/arm64/boot/Image $OUT/Image-v15
ls -la $OUT/Image-v15

echo "=== 4. repack boot-v15 (no ramoops cmdline; DTB has plain no-map node) ==="
python3 $BASE/tools/repack_boot.py $BASE/apatch_patched_11266_0.13.5_clte.img $OUT/Image-v15 $OUT/boot-v15.img $OUT/dtbs/tail-new.bin
ls -la $OUT/boot-v15.img
echo V15_DONE
