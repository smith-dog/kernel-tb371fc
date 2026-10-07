// SPDX-License-Identifier: GPL-2.0
/*
 * t58_canary - boot-depth beacon for a board with no usable console.
 *
 * Each initcall level arms a timer that force-reboots the machine after its own
 * delay. Only the rungs that were actually armed can fire, so the period of the
 * resulting reboot loop says how deep boot got:
 *
 *   cycle ~20 s  -> late_initcall reached   (level 7)
 *   cycle ~30 s  -> device_initcall reached (level 6)
 *   cycle ~80 s  -> subsys_initcall reached (level 1), died before level 6
 *   cycle ~140 s -> early_initcall reached  (level 0), died before level 1
 *   no cycle     -> died before any initcall, or IRQs/timers are gone
 *
 * Timers, not kthreads: #220 showed a worker created at initcall level 0 could
 * vanish for reasons still unpinned, and a kthread here would make "no cycle"
 * ambiguous. The timers must NOT be deferrable: on an idle or NOHZ CPU a
 * deferrable timer can be postponed indefinitely, so a merely parked machine
 * would fake a "never reached this level" reading - that mistake voided round A.
 * emergency_restart() neither syncs nor touches the console, so it
 * still works when storage and the slow paths are gone.
 *
 * Dev-branch diagnostic asset (AGENTS 8.4); must be off in releases.
 */
#define pr_fmt(fmt) "t58_canary: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/timer.h>

struct rung {
	struct timer_list t;
	const char *name;
	unsigned int delay_s;
};

static void fire(struct timer_list *t)
{
	struct rung *rung = from_timer(rung, t, t);

	pr_emerg("rung %s firing\n", rung->name);
	emergency_restart();
}

static void __init arm(struct rung *rung)
{
	/* T58: all rungs disarmed - the machine now survives device_initcall,
	 * so the 1s rung was masking everything after it. Let it run to its
	 * real freeze; the flight recorder keeps the final log. */
	return;
	timer_setup(&rung->t, fire, 0);
	rung->t.expires = jiffies + msecs_to_jiffies(rung->delay_s * MSEC_PER_SEC);
	add_timer(&rung->t);
	pr_info("rung %s armed (%u s)\n", rung->name, rung->delay_s);
}

static struct rung rung_late   = { .name = "level7-late",   .delay_s = 10 };
static struct rung rung_dev    = { .name = "level6-device", .delay_s = 1 };
static struct rung rung_subsys = { .name = "level1-subsys", .delay_s = 70 };
static struct rung rung_early  = { .name = "level0-early",  .delay_s = 60 };

static int __init early_arm(void)
{
	arm(&rung_early);
	return 0;
}
early_initcall(early_arm);

/*
 * Ladder round A: only two rungs are armed - level 0 at 60 s and level 6 at 1 s
 * (deepest first, so a reboot at ~10 s vs ~70 s vs never is a three-way readout
 * of "kernel alive past device_initcall" / "alive at level 0 only" / "no timers
 * at all"). Re-enable subsys/late arms when the depth question moves again.
 */
static int __init subsys_arm(void)
{
	arm(&rung_subsys);
	return 0;
}
subsys_initcall(subsys_arm);

static int __init dev_arm(void)
{
	arm(&rung_dev);
	return 0;
}
device_initcall(dev_arm);

static int __init late_arm(void)
{
	arm(&rung_late);
	return 0;
}
late_initcall(late_arm);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB371FC boot-depth beacon (reboot period == deepest initcall reached)");
