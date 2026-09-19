// SPDX-License-Identifier: GPL-2.0-only
/*
 * ksu_sym.c — p130: KernelSU LKM 符号导出垫片。
 * backslashxx/KernelSU 32630 以模块形态运行所需、但主线不导出的符号。
 * 内核侧一次性改动；此后 KSU 迭代仅换 ksu.ko，不再重编内核。
 * 导出清单来自 drivers/kernelsu/ksu.ko 的 modpost undefined 报告。
 */
#include <linux/module.h>
#include <linux/uidgid.h>
#include <linux/cred.h>
#include <linux/mount.h>
#include <linux/syscalls.h>

extern void free_uid(struct user_struct *up);
extern struct user_struct *alloc_uid(kuid_t uid);
extern int do_mount(const char *dev_name, const char __user *dir_path,
		const char *type_page, unsigned long flags, void *data_page);
extern long __arm64_sys_umount(const struct pt_regs *regs);

EXPORT_SYMBOL_GPL(free_uid);
EXPORT_SYMBOL_GPL(alloc_uid);
EXPORT_SYMBOL_GPL(do_mount);
EXPORT_SYMBOL_GPL(__arm64_sys_umount);

MODULE_LICENSE("GPL");
