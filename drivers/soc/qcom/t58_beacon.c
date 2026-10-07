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
#include <linux/io.h>
#include <linux/sizes.h>
#include <asm/early_ioremap.h>

/*
 * Default stage baked at build time (sed this line per probe): stages >= 20
 * sit BEFORE parse_early_param, where the cmdline is not yet parsed, so the
 * selection cannot come from t58stage= on the command line.
 */
static int t58stage = -1;

/*
 * PSCI SYSTEM_RESET via raw SMC (stock DTB: arm,psci-1.0, method="smc";
 * the same call the psci driver issues, proven by the C0 reboot loop).
 * Works from any EL1 context with the MMU on - no ioremap, no DT scan, no
 * registered handler needed. HVC fallback in case the conduit ever differs.
 */
static noinline void t58_smc_reset(void)
{
	register long x0 asm("x0") = 0x84000009;	/* PSCI 0.2 SYSTEM_RESET */

	asm volatile("smc #0" : : "r"(x0) : "x1", "x2", "x3", "memory");
	asm volatile("hvc #0" : : "r"(x0) : "x1", "x2", "x3", "memory");
}

/*
 * Raw WDT bite: the pre-psci_dt_init stages have no arm_pm_restart handler,
 * so emergency_restart() would halt at "Reboot failed". Biting the qcom WDT
 * directly (same registers the vendor watchdog_v2 driver uses) resets the
 * machine from anywhere after early_ioremap_init(). Base 0x17C10000 is the
 * stock DTB's qcom,wdt@17c10000 (qcom,msm-watchdog).
 */
static void t58_wdt_bite(void)
{
	void __iomem *base = early_ioremap(0x17C10000UL, SZ_4K);

	if (!base)
		return;
	/* WDT0_BITE_TIME = 1 tick, then RST arms it: bite within ~ms */
	__raw_writel(1, base + 0x14);
	mb();
	__raw_writel(1, base + 0x04);
	mb();
	for (;;)
		cpu_relax();
}

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

	/*
	 * NO printk here: once the console= alias registers earlycon inside
	 * parse_early_param, a printk from the beacon itself goes out over the
	 * UART and a hang there fakes a "frozen" reading for a healthy boot
	 * (the whole S-ladder was voided by this). The reboot loop period is
	 * the only observable; it needs no output.
	 */
	for (i = 0; i < 300000000UL; i++)
		;

	/*
	 * Reset via the proven raw PSCI SMC at every stage. Do NOT branch on
	 * stage number: stages 12-18 sit BEFORE psci_dt_init registers
	 * arm_pm_restart, so emergency_restart() there halts at "Reboot
	 * failed" and fakes a frozen reading for a healthy machine.
	 */
	t58_smc_reset();
	/* belt and braces if the SMC ever returns */
	emergency_restart();
}
