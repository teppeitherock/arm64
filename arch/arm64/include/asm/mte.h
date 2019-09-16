/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_MTE_H
#define __ASM_MTE_H

#ifndef __ASSEMBLY__

#include <linux/sched.h>

/* Memory Tagging API */
int mte_memcmp_pages(const void *page1_addr, const void *page2_addr);

#ifdef CONFIG_ARM64_MTE
void flush_mte_state(void);
void mte_thread_switch(struct task_struct *next);
#else
static inline void flush_mte_state(void)
{
}
static inline void mte_thread_switch(struct task_struct *next)
{
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ASM_MTE_H  */
