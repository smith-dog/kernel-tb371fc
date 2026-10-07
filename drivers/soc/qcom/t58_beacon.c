// SPDX-License-Identifier: GPL-2.0
/*
 * t58_beacon - start_kernel-milestone reboot beacon (companion to t58_canary).
 *
 * The canary only arms at initcall levels. This unit lets a cmdline-selected
 * milestone INSIDE start_kernel reboot the machine, so the freeze point can be
 * bisected without a console:
 *
 *   t58stage=N on the cmdline arms stage N. When start_kernel reaches the
 *   matching t58_stage_hit() call site, it busy-waits ~2 s (no tick needed)
 *   and calls emergency_restart(). A reboot loop  = boot reached that stage.
 *   A silent freeze = boot died before it.
 *
 * Valid from stage positions after psci_dt_init() (inside setup_arch), which
 * is where arm_pm_restart gets its PSCI handler - emergency_restart() falls
 * back to "Reboot failed -- System halted" if no handler is registered, so
 * stages are only placed after that point.
 *
 * Stages (dev init/main.c):
 *   1 right after setup_arch()
 *   2 after mm_init()
 *   3 after sched_init()
 *   4 after tick_init()          (covers early_irq_init/init_IRQ)
 *   5 after timekeeping_init()   (covers init_timers/hrtimers/softirq)
 *   6 after time_init()
 *   7 after calibrate_delay()    (covers console_init)
 * Level 0+ remains covered by t58_canary.
 *
 * Dev-branch diagnostic asset (AGENTS 8.4); must be off in releases.
 */
#define pr_fmt(fmt) "t58_beacon: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/reboot.h>

static int t58stage = -1;

static int __init t58stage_setup(char *s)
{
	int v;

	if (kstrtoint(s, 10, &v) == 0)
		t58stage = v;
	pr_info("cmdline selected stage %d\n", t58stage);
	return 1;
}
early_param("t58stage=", t58stage_setup);

void t58_stage_hit(int stage)
{
	volatile unsigned long i;

	if (t58stage != stage)
		return;

	pr_emerg("stage %d hit - busy wait then restart\n", stage);
	/* crude busy wait: the loop period only has to be visible on screen */
	for (i = 0; i < 300000000UL; i++)
		;
	pr_emerg("stage %d restarting\n", stage);
	emergency_restart();
}
