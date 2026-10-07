// SPDX-License-Identifier: GPL-2.0
/*
 * t58_tasks - periodic all-task stack dump for a userspace-stuck boot.
 *
 * The gadget-freeze lives show a kernel that is alive (the flight recorder
 * keeps snapshotting) but a userspace that stalls around the bpfloader/netd
 * phase. hung_task cannot see the stuck state (it is not D-state), and a
 * workqueue-based dump never runs when system_wq is wedged (both observed),
 * so this diagnostic runs on its OWN kthread and dumps EVERY task's stack in
 * recorder-sized chunks: one chunk every ~1.15 s so each chunk lands in a
 * separate 1 s flight-recorder snapshot. Two passes: +12 s and +42 s.
 *
 * Dev-branch diagnostic asset (AGENTS 8.4); must be off in releases.
 */
#define pr_fmt(fmt) "t58_tasks: " fmt

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>

static void t58_dump_pass(const char *tag)
{
	struct task_struct *g, *p;
	int n = 0;

	pr_info("=== begin full task dump (%s) ===\n", tag);
	rcu_read_lock();
	for_each_process_thread(g, p) {
		sched_show_task(p);
		if (++n % 6 == 0) {
			rcu_read_unlock();
			msleep(1150);
			rcu_read_lock();
		}
	}
	rcu_read_unlock();
	pr_info("=== end full task dump (%s, %d tasks) ===\n", tag, n);
}

static int t58_tasks_kthread(void *arg)
{
	ssleep(12);
	t58_dump_pass("+12s");
	ssleep(30);
	t58_dump_pass("+42s");
	return 0;
}

static struct task_struct *t58_tasks_thread;

static int __init t58_tasks_init(void)
{
	t58_tasks_thread = kthread_run(t58_tasks_kthread, NULL, "t58_tasks");
	if (IS_ERR(t58_tasks_thread))
		return PTR_ERR(t58_tasks_thread);
	pr_info("task dump kthread started, first dump in 12 s\n");
	return 0;
}
late_initcall(t58_tasks_init);
