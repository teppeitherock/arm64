/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_MMAN_H__
#define __ASM_MMAN_H__

#include <uapi/asm/mman.h>

/*
 * There are two conditions required for returning a Normal Tagged memory type
 * in arch_vm_get_page_prot(): (1) the user requested it via PROT_MTE passed
 * to mmap() or mprotect() and (2) the corresponding vma supports MTE. We
 * register (1) as VM_MTE in the vma->vm_flags and (2) as VM_MTE_ALLOWED. Note
 * that the latter can only be set during the mmap() call since mprotect()
 * does not accept MAP_* flags.
 */
static inline unsigned long arch_calc_vm_prot_bits(unsigned long prot,
						   unsigned long pkey)
{
	if (!system_supports_mte())
		return 0;

	if (prot & PROT_MTE)
		return VM_MTE;

	return 0;
}
#define arch_calc_vm_prot_bits arch_calc_vm_prot_bits

static inline unsigned long arch_calc_vm_flag_bits(unsigned long flags)
{
	if (!system_supports_mte())
		return 0;

	/*
	 * Only allow MTE on anonymous mappings as these are guaranteed to be
	 * backed by tags-capable memory. The vm_flags may be overridden by a
	 * filesystem supporting MTE (RAM-based).
	 */
	if (flags & MAP_ANONYMOUS)
		return VM_MTE_ALLOWED;

	return 0;
}
#define arch_calc_vm_flag_bits arch_calc_vm_flag_bits

static inline pgprot_t arch_vm_get_page_prot(unsigned long vm_flags)
{
	/*
	 * Checking for VM_MTE only is sufficient since arch_validate_flags()
	 * does not permit (VM_MTE & !VM_MTE_ALLOWED).
	 */
	return (vm_flags & VM_MTE) ?
		__pgprot(PTE_ATTRINDX(MT_NORMAL_TAGGED)) :
		__pgprot(0);
}
#define arch_vm_get_page_prot arch_vm_get_page_prot

static inline bool arch_validate_prot(unsigned long prot, unsigned long addr)
{
	unsigned long supported = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM;

	if (system_supports_mte())
		supported |= PROT_MTE;

	return (prot & ~supported) == 0;
}
#define arch_validate_prot arch_validate_prot

static inline bool arch_validate_flags(unsigned long flags)
{
	if (!system_supports_mte())
		return true;

	/* only allow VM_MTE if VM_MTE_ALLOWED has been set previously */
	return !(flags & VM_MTE) || (flags & VM_MTE_ALLOWED);
}
#define arch_validate_flags arch_validate_flags

#endif /* !__ASM_MMAN_H__ */
