// SPDX-License-Identifier: GPL-2.0
/*
 * t58_tasks - periodic all-task stack dump for a userspace-stuck boot.
 *
 * The gadget-freeze lives show a kernel that is alive (the flight recorder
 * keeps snapshotting) but a userspace that stalls around the bpfloader/netd
 * phase. hung_task cannot see the stuck state (it is not D-state), so this
 * diagnostic dumps EVERY task's stack in recorder-sized chunks: one chunk
 * every ~1.15 s so each chunk lands in a separate 1 s flight-recorder
 * snapshot, and the whole task table can be read back from flash.
 *
 * Runs once, T58_TASKS_DELAY_S seconds after late_initcall.
 *
 * Dev-branch diagnostic asset (AGENTS 8.4); must be off in releases.
 */
#define pr_fmt(fmt) "t58_tasks: " fmt

#define T58_TASKS_DELAY_S 6

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

static void t58_dump_work(struct work_struct *w)
{
	struct task_struct *g, *p;
	int n = 0;

	pr_info("=== begin full task dump ===\n");
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
	pr_info("=== end full task dump (%d tasks) ===\n", n);
}

static DECLARE_DELAYED_WORK(t58_dump_workq, t58_dump_work);

static int __init t58_tasks_init(void)
{
	schedule_delayed_work(&t58_dump_workq,
			      msecs_to_jiffies(T58_TASKS_DELAY_S * 1000));
	pr_info("task dump scheduled in %d s\n", T58_TASKS_DELAY_S);
	return 0;
}
late_initcall(t58_tasks_init);
