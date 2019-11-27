/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 ARM Ltd.
 */
#include <linux/linkage.h>

#include <asm/assembler.h>

/*
 * Compare tags of two pages
 *   x0 - page1 address
 *   x1 - page2 address
 * Returns:
 *   w0 - negative, zero or positive value if the tag in the first page is
 *	  less than, equal to or greater than the tag in the second page
 */
ENTRY(mte_memcmp_pages)
	multitag_transfer_size x7, x5
1:
	ldgm	x2, [x0]
	ldgm	x3, [x1]

	eor	x4, x2, x3
	cbnz	x4, 2f

	add	x0, x0, x7
	add	x1, x1, x7

	tst	x0, #(PAGE_SIZE - 1)
	b.ne	1b

	mov	w0, #0
	ret
2:
	rbit	x4, x4
	clz	x4, x4			// count the least significant equal bits
	and	x4, x4, #~3		// round down to a multiple of 4 (bits per tag)

	lsr	x2, x2, x4		// remove equal tags
	lsr	x3, x3, x4

	lsl	w2, w2, #28		// compare the differing tags
	sub	w0, w2, w3, lsl #28

	ret
ENDPROC(mte_memcmp_pages)
