/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/mem_manage.h>
#include <zephyr/toolchain.h>
#include <mmu.h>
#include <zephyr/linker/sections.h>

#define BASE_FLAGS	(K_MEM_CACHE_WB)
volatile bool expect_fault;

__pinned_noinit
static uint8_t __aligned(CONFIG_MMU_PAGE_SIZE)
			test_page[2 * CONFIG_MMU_PAGE_SIZE];

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *pEsf)
{
	printk("Caught system error -- reason %d\n", reason);

	if (expect_fault && reason == 0) {
		expect_fault = false;
		ztest_test_pass();
	} else {
		printk("Unexpected fault during test");
		k_fatal_halt(reason);
	}
}


/* z_phys_map() doesn't have alignment requirements, any oddly-sized buffer
 * can get mapped. This will span two pages.
 */
#define BUF_SIZE	5003
#define BUF_OFFSET	1238

/**
 * Show that mapping an irregular size buffer works and RW flag is respected
 *
 * @ingroup kernel_memprotect_tests
 */
ZTEST(mem_map, test_z_phys_map_rw)
{
	uint8_t *mapped_rw, *mapped_ro;
	uint8_t *buf = test_page + BUF_OFFSET;

	expect_fault = false;

	/* Map in a page that allows writes */
	z_phys_map(&mapped_rw, z_mem_phys_addr(buf),
		   BUF_SIZE, BASE_FLAGS | K_MEM_PERM_RW);

	/* Initialize buf with some bytes */
	for (int i = 0; i < BUF_SIZE; i++) {
		mapped_rw[i] = (uint8_t)(i % 256);
	}

	/* Map again this time only allowing reads */
	z_phys_map(&mapped_ro, z_mem_phys_addr(buf),
		   BUF_SIZE, BASE_FLAGS);

	/* Check that the mapped area contains the expected data. */
	for (int i = 0; i < BUF_SIZE; i++) {
		zassert_equal(buf[i], mapped_ro[i],
			      "unequal byte at index %d", i);
	}

	/* This should explode since writes are forbidden */
	expect_fault = true;
	mapped_ro[0] = 42;

	printk("shouldn't get here\n");
	ztest_test_fail();
}

/**
 * Show that memory mapping doesn't have unintended side effects
 *
 * @ingroup kernel_memprotect_tests
 */
ZTEST(mem_map, test_z_phys_map_side_effect)
{
	uint8_t *mapped;

	expect_fault = false;

	/* z_phys_map() is supposed to always create fresh mappings.
	 * Show that by mapping test_page to an RO region, we can still
	 * modify test_page.
	 */
	z_phys_map(&mapped, z_mem_phys_addr(test_page),
		   sizeof(test_page), BASE_FLAGS);

	/* Should NOT fault */
	test_page[0] = 42;

	/* Should fault */
	expect_fault = true;
	mapped[0] = 42;
	printk("shouldn't get here\n");
	ztest_test_fail();
}

/**
 * Test that z_phys_unmap() unmaps the memory and it is no longer
 * accessible afterwards.
 *
 * @ingroup kernel_memprotect_tests
 */
ZTEST(mem_map, test_z_phys_unmap)
{
	uint8_t *mapped;

	expect_fault = false;

	/* Map in a page that allows writes */
	z_phys_map(&mapped, z_mem_phys_addr(test_page),
		   sizeof(test_page), BASE_FLAGS | K_MEM_PERM_RW);

	/* Should NOT fault */
	mapped[0] = 42;

	/* Unmap the memory */
	z_phys_unmap(mapped, sizeof(test_page));

	/* Should fault since test_page is no longer accessible */
	expect_fault = true;
	mapped[0] = 42;
	printk("shouldn't get here\n");
	ztest_test_fail();
}

/**
 * Test that the "before" guard page is in place for k_mem_map().
 */
ZTEST(mem_map_api, test_k_mem_map_guard_before)
{
	uint8_t *mapped;

	expect_fault = false;

	mapped = k_mem_map(CONFIG_MMU_PAGE_SIZE, K_MEM_PERM_RW);
	zassert_not_null(mapped, "failed to map memory");
	printk("mapped a page: %p - %p\n", mapped,
		mapped + CONFIG_MMU_PAGE_SIZE);

	/* Should NOT fault */
	mapped[0] = 42;

	/* Should fault here in the guard page location */
	expect_fault = true;
	mapped -= sizeof(void *);

	printk("trying to access %p\n", mapped);

	mapped[0] = 42;
	printk("shouldn't get here\n");
	ztest_test_fail();
}

/**
 * Test that the "after" guard page is in place for k_mem_map().
 */
ZTEST(mem_map_api, test_k_mem_map_guard_after)
{
	uint8_t *mapped;

	expect_fault = false;

	mapped = k_mem_map(CONFIG_MMU_PAGE_SIZE, K_MEM_PERM_RW);
	zassert_not_null(mapped, "failed to map memory");
	printk("mapped a page: %p - %p\n", mapped,
		mapped + CONFIG_MMU_PAGE_SIZE);

	/* Should NOT fault */
	mapped[0] = 42;

	/* Should fault here in the guard page location */
	expect_fault = true;
	mapped += CONFIG_MMU_PAGE_SIZE + sizeof(void *);

	printk("trying to access %p\n", mapped);

	mapped[0] = 42;
	printk("shouldn't get here\n");
	ztest_test_fail();
}


ZTEST_SUITE(mem_map, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(mem_map_api, NULL, NULL, NULL, NULL, NULL);
