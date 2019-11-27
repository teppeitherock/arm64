// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 ARM Ltd.
 */

#include <linux/mm.h>
#include <linux/string.h>

#include <asm/cpufeature.h>
#include <asm/mte.h>

int memcmp_pages(struct page *page1, struct page *page2)
{
	char *addr1, *addr2;
	int ret;

	addr1 = page_address(page1);
	addr2 = page_address(page2);

	ret = memcmp(addr1, addr2, PAGE_SIZE);
	/* if page content identical, check the tags */
	if (ret == 0 && system_supports_mte())
		ret = mte_memcmp_pages(addr1, addr2);

	return ret;
}
