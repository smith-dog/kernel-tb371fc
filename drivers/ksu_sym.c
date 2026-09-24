// SPDX-License-Identifier: GPL-2.0-only
/*
 * ksu_sym.c — p130: KernelSU LKM symbol export shim (auto-generated).
 * Exports symbols required by backslashxx/KernelSU ksu.ko that the
 * mainline does not EXPORT_SYMBOL. One-time kernel-side change; KSU
 * iterations afterwards only replace ksu.ko.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/mount.h>
#include <linux/pid_namespace.h>
#include <linux/utsname.h>
#include <linux/key-type.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/selinux.h>
#include <asm/insn.h>

/* typed declarations resolved at link time (vmlinux) */
extern void free_uid(struct user_struct *);
extern struct user_struct *alloc_uid(kuid_t);
extern long __arm64_sys_umount(const struct pt_regs *);
extern long __arm64_sys_close(const struct pt_regs *);
extern long __arm64_sys_execve(const struct pt_regs *);
extern long __arm64_sys_execveat(const struct pt_regs *);
extern long __arm64_compat_sys_execve(const struct pt_regs *);
extern long __arm64_compat_sys_execveat(const struct pt_regs *);
extern long __arm64_sys_faccessat(const struct pt_regs *);
extern long __arm64_sys_finit_module(const struct pt_regs *);
extern long __arm64_sys_init_module(const struct pt_regs *);
extern long __arm64_sys_read(const struct pt_regs *);
extern long __arm64_sys_reboot(const struct pt_regs *);
extern long __arm64_sys_setns(const struct pt_regs *);
extern long __arm64_sys_newfstat(const struct pt_regs *);
extern long __arm64_sys_newfstatat(const struct pt_regs *);
extern long __arm64_sys_fstat64(const struct pt_regs *);
extern long __arm64_sys_fstatat64(const struct pt_regs *);
extern void change_pid(struct task_struct *, enum pid_type, struct pid *);
extern struct task_struct *find_task_by_vpid(pid_t);
extern u32 aarch64_insn_gen_branch_imm(unsigned long pc, unsigned long addr, enum aarch64_insn_branch_type type);
extern int avc_ss_reset(u32 seqno);
extern int avtab_alloc(struct avtab *, u32);
extern void avtab_destroy(struct avtab *);
extern struct avtab_node *avtab_insert_nonunique(struct avtab *, struct avtab_key *, struct avtab_datum *);
extern struct avtab_node *avtab_search_node(struct avtab *, struct avtab_key *);
extern struct avtab_node *avtab_search_node_next(struct avtab_node *, int);
extern int ebitmap_get_bit(struct ebitmap *, unsigned long);
extern int ebitmap_set_bit(struct ebitmap *, unsigned long, int);
extern int hashtab_insert(struct hashtab *, struct hashtab_key, struct hashtab_datum);
extern void *hashtab_search(struct hashtab *, struct hashtab_key);
extern int install_session_keyring_to_cred(struct cred *, struct key *);
extern void ext4_unregister_sysfs(struct super_block *);
extern rwlock_t tasklist_lock;
extern struct rw_semaphore uts_sem;
extern struct selinux_state selinux_state;
extern struct security_hook_heads security_hook_heads;
extern int selinux_status_update_policyload(int);
extern void selnl_notify_policyload(u32);
extern struct page *selinux_kernel_status_page(struct selinux_state *);
extern struct sys_call_table_def { unsigned long __null_; } sys_call_table[] ;
extern unsigned long compat_sys_call_table[];

EXPORT_SYMBOL_GPL(__arm64_compat_sys_execve);
EXPORT_SYMBOL_GPL(__arm64_compat_sys_execveat);
EXPORT_SYMBOL_GPL(__arm64_sys_execve);
EXPORT_SYMBOL_GPL(__arm64_sys_execveat);
EXPORT_SYMBOL_GPL(__arm64_sys_faccessat);
EXPORT_SYMBOL_GPL(__arm64_sys_finit_module);
EXPORT_SYMBOL_GPL(__arm64_sys_fstat64);
EXPORT_SYMBOL_GPL(__arm64_sys_fstatat64);
EXPORT_SYMBOL_GPL(__arm64_sys_init_module);
EXPORT_SYMBOL_GPL(__arm64_sys_newfstat);
EXPORT_SYMBOL_GPL(__arm64_sys_newfstatat);
EXPORT_SYMBOL_GPL(__arm64_sys_read);
EXPORT_SYMBOL_GPL(__arm64_sys_reboot);
EXPORT_SYMBOL_GPL(__arm64_sys_setns);
EXPORT_SYMBOL_GPL(__arm64_sys_umount);
EXPORT_SYMBOL_GPL(__arm64_sys_close);
EXPORT_SYMBOL_GPL(_etext);
EXPORT_SYMBOL_GPL(_stext);
EXPORT_SYMBOL_GPL(aarch64_get_branch_offset);
EXPORT_SYMBOL_GPL(aarch64_insn_gen_branch_imm);
EXPORT_SYMBOL_GPL(aarch64_insn_patch_text);
EXPORT_SYMBOL_GPL(alloc_uid);
EXPORT_SYMBOL_GPL(avc_ss_reset);
EXPORT_SYMBOL_GPL(avtab_alloc);
EXPORT_SYMBOL_GPL(avtab_destroy);
EXPORT_SYMBOL_GPL(avtab_insert_nonunique);
EXPORT_SYMBOL_GPL(avtab_search_node);
EXPORT_SYMBOL_GPL(avtab_search_node_next);
EXPORT_SYMBOL_GPL(change_pid);
EXPORT_SYMBOL_GPL(compat_sys_call_table);
EXPORT_SYMBOL_GPL(do_mount);
EXPORT_SYMBOL_GPL(ebitmap_get_bit);
EXPORT_SYMBOL_GPL(ebitmap_set_bit);
EXPORT_SYMBOL_GPL(ext4_unregister_sysfs);
EXPORT_SYMBOL_GPL(find_task_by_vpid);
EXPORT_SYMBOL_GPL(free_uid);
EXPORT_SYMBOL_GPL(hashtab_insert);
EXPORT_SYMBOL_GPL(hashtab_search);
EXPORT_SYMBOL_GPL(install_session_keyring_to_cred);
EXPORT_SYMBOL_GPL(kallsyms_lookup_name);
EXPORT_SYMBOL_GPL(ksys_unshare);
EXPORT_SYMBOL_GPL(security_hook_heads);
EXPORT_SYMBOL_GPL(selinux_kernel_status_page);
EXPORT_SYMBOL_GPL(selinux_state);
EXPORT_SYMBOL_GPL(selinux_status_update_policyload);
EXPORT_SYMBOL_GPL(selnl_notify_policyload);
EXPORT_SYMBOL_GPL(sys_call_table);
EXPORT_SYMBOL_GPL(tasklist_lock);
EXPORT_SYMBOL_GPL(uts_sem);

MODULE_LICENSE("GPL");
