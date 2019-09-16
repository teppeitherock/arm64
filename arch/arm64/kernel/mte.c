// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 ARM Ltd.
 */

#include <linux/thread_info.h>

#include <asm/cpufeature.h>
#include <asm/mte.h>
#include <asm/sysreg.h>

void flush_mte_state(void)
{
	if (!system_supports_mte())
		return;

	/* clear any pending asynchronous tag fault */
	clear_thread_flag(TIF_MTE_ASYNC_FAULT);
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
}
