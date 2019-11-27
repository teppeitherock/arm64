/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__ASM_MMAN_H
#define _UAPI__ASM_MMAN_H

#include <asm-generic/mman.h>

/*
 * The generic mman.h file reserves 0x10 and 0x20 for arch-specific PROT_*
 * flags.
 */
/* 0x10 reserved for PROT_BTI */
#define PROT_MTE	 0x20		/* Normal Tagged mapping */

#endif /* !_UAPI__ASM_MMAN_H */
