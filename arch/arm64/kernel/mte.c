// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 ARM Ltd.
 */

#include <linux/prctl.h>
#include <linux/sched.h>
#include <linux/thread_info.h>

#include <asm/cpufeature.h>
#include <asm/mte.h>
#include <asm/sysreg.h>

static void update_sctlr_el1_tcf0(u64 tcf0)
{
	/* ISB required for the kernel uaccess routines */
	sysreg_clear_set(sctlr_el1, SCTLR_EL1_TCF0_MASK, tcf0);
	isb();
}

static void set_sctlr_el1_tcf0(u64 tcf0)
{
	/*
	 * mte_thread_switch() checks current->thread.sctlr_tcf0 as an
	 * optimisation. Disable preemption so that it does not see
	 * the variable update before the SCTLR_EL1.TCF0 one.
	 */
	preempt_disable();
	current->thread.sctlr_tcf0 = tcf0;
	update_sctlr_el1_tcf0(tcf0);
	preempt_enable();
}

static void update_gcr_el1_excl(u64 incl)
{
	u64 excl = ~incl & SYS_GCR_EL1_EXCL_MASK;

	/*
	 * Note that 'incl' is an include mask (controlled by the user via
	 * prctl()) while GCR_EL1 accepts an exclude mask.
	 * No need for ISB since this only affects EL0 currently, implicit
	 * with ERET.
	 */
	sysreg_clear_set_s(SYS_GCR_EL1, SYS_GCR_EL1_EXCL_MASK, excl);
}

static void set_gcr_el1_excl(u64 incl)
{
	current->thread.gcr_incl = incl;
	update_gcr_el1_excl(incl);
}

void flush_mte_state(void)
{
	if (!system_supports_mte())
		return;

	/* clear any pending asynchronous tag fault */
	clear_thread_flag(TIF_MTE_ASYNC_FAULT);
	/* disable tag checking */
	set_sctlr_el1_tcf0(0);
	/* reset tag generation mask */
	set_gcr_el1_excl(0);
}

void mte_thread_switch(struct task_struct *next)
{
	u64 tfsr;

	if (!system_supports_mte())
		return;

	/*
	 * Check for asynchronous tag check faults from the uaccess routines
	 * before switching to the next thread.
	 */
	tfsr = read_sysreg_s(SYS_TFSRE0_EL1);
	if (tfsr & SYS_TFSR_EL1_TF0) {
		set_thread_flag(TIF_MTE_ASYNC_FAULT);
		write_sysreg_s(0, SYS_TFSRE0_EL1);
	}

	/* avoid expensive SCTLR_EL1 accesses if no change */
	if (current->thread.sctlr_tcf0 != next->thread.sctlr_tcf0)
		update_sctlr_el1_tcf0(next->thread.sctlr_tcf0);
	update_gcr_el1_excl(next->thread.gcr_incl);
}

long set_mte_ctrl(unsigned long arg)
{
	u64 tcf0;

	if (!system_supports_mte())
		return 0;

	switch (arg & PR_MTE_TCF_MASK) {
	case PR_MTE_TCF_NONE:
		tcf0 = SCTLR_EL1_TCF0_NONE;
		break;
	case PR_MTE_TCF_SYNC:
		tcf0 = SCTLR_EL1_TCF0_SYNC;
		break;
	case PR_MTE_TCF_ASYNC:
		tcf0 = SCTLR_EL1_TCF0_ASYNC;
		break;
	default:
		return -EINVAL;
	}

	set_sctlr_el1_tcf0(tcf0);
	set_gcr_el1_excl((arg & PR_MTE_TAG_MASK) >> PR_MTE_TAG_SHIFT);

	return 0;
}

long get_mte_ctrl(void)
{
	unsigned long ret;

	if (!system_supports_mte())
		return 0;

	ret = current->thread.gcr_incl << PR_MTE_TAG_SHIFT;

	switch (current->thread.sctlr_tcf0) {
	case SCTLR_EL1_TCF0_NONE:
		return PR_MTE_TCF_NONE;
	case SCTLR_EL1_TCF0_SYNC:
		ret |= PR_MTE_TCF_SYNC;
		break;
	case SCTLR_EL1_TCF0_ASYNC:
		ret |= PR_MTE_TCF_ASYNC;
		break;
	}

	return ret;
}
