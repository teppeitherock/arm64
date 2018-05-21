// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 ARM Ltd, All Rights Reserved.
 */

#include <linux/sched.h>
#include <linux/thread_info.h>

#include <asm/cpufeature.h>

/*
 * prctl interface for SSBD
 * FIXME: Drop the below ifdefery once the common interface has been merged.
 */
#ifdef PR_SPEC_STORE_BYPASS
static int ssbd_prctl_set(struct task_struct *task, unsigned long ctrl)
{
	int state = arm64_get_ssbd_state();

	/* Unsupported or already mitigated */
	if (state == ARM64_SSBD_UNKNOWN)
		return -EINVAL;
	if (state == ARM64_SSBD_MITIGATED)
		return -EPERM;

	/*
	 * Things are a bit backward here: the arm64 internal API
	 * *enables the mitigation* when the userspace API *disables
	 * speculation*. So much fun.
	 */
	switch (ctrl) {
	case PR_SPEC_ENABLE:
		/* If speculation is force disabled, enable is not allowed */
		if (state == ARM64_SSBD_FORCE_ENABLE ||
		    task_spec_ssb_force_disable(task))
			return -EPERM;
		task_clear_spec_ssb_disable(task);
		clear_tsk_thread_flag(task, TIF_SSBD);
		break;
	case PR_SPEC_DISABLE:
		if (state == ARM64_SSBD_FORCE_DISABLE)
			return -EPERM;
		task_set_spec_ssb_disable(task);
		set_tsk_thread_flag(task, TIF_SSBD);
		break;
	case PR_SPEC_FORCE_DISABLE:
		if (state == ARM64_SSBD_FORCE_DISABLE)
			return -EPERM;
		task_set_spec_ssb_disable(task);
		task_set_spec_ssb_force_disable(task);
		set_tsk_thread_flag(task, TIF_SSBD);
		break;
	default:
		return -ERANGE;
	}

	return 0;
}

int arch_prctl_spec_ctrl_set(struct task_struct *task, unsigned long which,
			     unsigned long ctrl)
{
	switch (which) {
	case PR_SPEC_STORE_BYPASS:
		return ssbd_prctl_set(task, ctrl);
	default:
		return -ENODEV;
	}
}

#ifdef CONFIG_SECCOMP
void arch_seccomp_spec_mitigate(struct task_struct *task)
{
	ssbd_prctl_set(task, PR_SPEC_FORCE_DISABLE);
}
#endif

static int ssbd_prctl_get(struct task_struct *task)
{
	switch (arm64_get_ssbd_state()) {
	case ARM64_SSBD_UNKNOWN:
		return -EINVAL;
	case ARM64_SSBD_FORCE_ENABLE:
		return PR_SPEC_DISABLE;
	case ARM64_SSBD_EL1_ENTRY:
		if (task_spec_ssb_force_disable(task))
			return PR_SPEC_PRCTL | PR_SPEC_FORCE_DISABLE;
		if (task_spec_ssb_disable(task))
			return PR_SPEC_PRCTL | PR_SPEC_DISABLE;
		return PR_SPEC_PRCTL | PR_SPEC_ENABLE;
	case ARM64_SSBD_FORCE_DISABLE:
		return PR_SPEC_ENABLE;
	default:
		return PR_SPEC_NOT_AFFECTED;
	}
}

int arch_prctl_spec_ctrl_get(struct task_struct *task, unsigned long which)
{
	switch (which) {
	case PR_SPEC_STORE_BYPASS:
		return ssbd_prctl_get(task);
	default:
		return -ENODEV;
	}
}
#endif	/* PR_SPEC_STORE_BYPASS */
