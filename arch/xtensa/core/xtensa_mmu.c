/*
 * Copyright (c) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/arch/xtensa/xtensa_mmu.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/mem_manage.h>
#include <xtensa_mmu_priv.h>
#include <adsp_memory.h>

LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);

/* Level 1 contains page table entries
 * necessary to map the page table itself.
 */
#define XTENSA_L1_PAGE_TABLE_ENTRIES 1024U

/* Level 2 contains page table entries
 * necessary to map the page table itself.
 */
#define XTENSA_L2_PAGE_TABLE_ENTRIES 1024U

BUILD_ASSERT(CONFIG_MMU_PAGE_SIZE == 0x1000,
	     "MMU_PAGE_SIZE value is invalid, only 4 kB pages are supported\n");

/*
 * Level 1 page table has to be 4Kb to fit into one of the wired entries.
 * All entries are initialized as INVALID, so an attempt to read an unmapped
 * area will cause a double exception.
 */
uint32_t l1_page_table[XTENSA_L1_PAGE_TABLE_ENTRIES] __aligned(KB(4));

/*
 * Each table in the level 2 maps a 4Mb memory range. It consists of 1024 entries each one
 * covering a 4Kb page.
 */
static uint32_t l2_page_tables[CONFIG_XTENSA_MMU_NUM_L2_TABLES][XTENSA_L2_PAGE_TABLE_ENTRIES]
				__aligned(KB(4));

/*
 * This additional variable tracks which l2 tables are in use. This is kept separated from
 * the tables to keep alignment easier.
 */
static ATOMIC_DEFINE(l2_page_tables_track, CONFIG_XTENSA_MMU_NUM_L2_TABLES);

extern char _heap_end[];
extern char _heap_start[];
extern char _data_start[];
extern char _data_end[];
extern char _bss_start[];
extern char _bss_end[];

__weak const struct xtensa_mmu_range xtensa_soc_mmu_ranges[] = {};
__weak int xtensa_soc_mmu_ranges_num;
/*
 * Static definition of all code & data memory regions of the
 * current Zephyr image. This information must be available &
 * processed upon MMU initialization.
 */

static const struct xtensa_mmu_range mmu_zephyr_ranges[] = {
	/*
	 * Mark the zephyr execution regions (data, bss, noinit, etc.)
	 * cacheable, read / write and non-executable
	 */
	{
		.start = (uint32_t)&_data_start,
		.end   = (uint32_t)&_data_end,
		.attrs = Z_XTENSA_MMU_W,
		.name = "data",
	},
	{
		.start = (uint32_t)&_bss_start,
		.end   = (uint32_t)&_bss_end,
		.attrs = Z_XTENSA_MMU_W,
		.name = "bss",
	},
	/* System heap memory */
	{
		.start = (uint32_t)&_heap_start,
		.end   = (uint32_t)&_heap_end,
		.attrs = Z_XTENSA_MMU_W,
		.name = "heap",
	},
	/* Mark text segment cacheable, read only and executable */
	{
		.start = (uint32_t)&__text_region_start,
		.end   = (uint32_t)&__text_region_end,
		.attrs = Z_XTENSA_MMU_X,
		.name = "text",
	},
	/* Mark rodata segment cacheable, read only and non-executable */
	{
		.start = (uint32_t)&__rodata_region_start,
		.end   = (uint32_t)&__rodata_region_end,
		.name = "rodata",
	},
};

static inline uint32_t *alloc_l2_table(void)
{
	uint16_t idx;

	for (idx = 0; idx < CONFIG_XTENSA_MMU_NUM_L2_TABLES; idx++) {
		if (!atomic_test_and_set_bit(l2_page_tables_track, idx)) {
			return (uint32_t *)&l2_page_tables[idx];
		}
	}

	return NULL;
}

static void map_memory_range(const struct xtensa_mmu_range *range)
{
	uint32_t page, *table;

	for (page = range->start; page < range->end; page += CONFIG_MMU_PAGE_SIZE) {
		uint32_t pte = Z_XTENSA_PTE(page, 0, range->attrs);
		uint32_t l2_pos = Z_XTENSA_L2_POS(page);
		uint32_t l1_pos = page >> 22;

			if (l1_page_table[l1_pos] == Z_XTENSA_MMU_ILLEGAL) {
				table  = alloc_l2_table();

			__ASSERT(table != NULL, "There is no l2 page table available to "
				"map 0x%08x\n", page);

			l1_page_table[l1_pos] =
				Z_XTENSA_PTE((uint32_t)table, 0, Z_XTENSA_MMU_CACHED_WT);
		}

		table = (uint32_t *)(l1_page_table[l1_pos] & Z_XTENSA_PTE_PPN_MASK);
		table[l2_pos] = pte;
	}
}

void z_xtensa_mmu_init(void)
{
	volatile uint8_t entry;
	uint32_t page, vecbase = VECBASE_RESET_PADDR_SRAM;

	for (page = 0; page < XTENSA_L1_PAGE_TABLE_ENTRIES; page++) {
		l1_page_table[page] = Z_XTENSA_MMU_ILLEGAL;
	}

	for (entry = 0; entry < ARRAY_SIZE(mmu_zephyr_ranges); entry++) {
		map_memory_range(&mmu_zephyr_ranges[entry]);
	}

	for (entry = 0; entry < xtensa_soc_mmu_ranges_num; entry++) {
		map_memory_range(&xtensa_soc_mmu_ranges[entry]);
	}

	xthal_dcache_all_writeback();

	/* Set the page table location in the virtual address */
	xtensa_ptevaddr_set((void *)Z_XTENSA_PTEVADDR);

	/* Next step is to invalidate the tlb entry that contains the top level
	 * page table. This way we don't cause a multi hit exception.
	 */
	xtensa_dtlb_entry_invalidate(Z_XTENSA_TLB_ENTRY(Z_XTENSA_PAGE_TABLE_VADDR, 6));
	xtensa_itlb_entry_invalidate(Z_XTENSA_TLB_ENTRY(Z_XTENSA_PAGE_TABLE_VADDR, 6));

	/* We are not using a flat table page, so we need to map
	 * only the top level page table (which maps the page table itself).
	 *
	 * Lets use one of the wired entry, so we never have tlb miss for
	 * the top level table.
	 */
	xtensa_dtlb_entry_write(Z_XTENSA_PTE((uint32_t)l1_page_table, 0, Z_XTENSA_MMU_CACHED_WB),
			Z_XTENSA_TLB_ENTRY(Z_XTENSA_PAGE_TABLE_VADDR, 7));

	/* Before invalidate the text region in the TLB entry 6, we need to
	 * map the exception vector into one of the wired entries to avoid
	 * a page miss for the exception.
	 */
	xtensa_itlb_entry_write_sync(
		Z_XTENSA_PTE(vecbase, 0,
			Z_XTENSA_MMU_X | Z_XTENSA_MMU_CACHED_WT),
		Z_XTENSA_TLB_ENTRY(
			Z_XTENSA_PTEVADDR + MB(4), 3));

	xtensa_dtlb_entry_write_sync(
		Z_XTENSA_PTE(vecbase, 0,
			Z_XTENSA_MMU_X | Z_XTENSA_MMU_CACHED_WT),
		Z_XTENSA_TLB_ENTRY(
			Z_XTENSA_PTEVADDR + MB(4), 3));

	__asm__ volatile("wsr.vecbase %0\n\t"
			:: "a"(Z_XTENSA_PTEVADDR + MB(4)));


	/* Finally, lets invalidate entries in the way 6 that are no longer
	 * needed. We keep 0x00000000 to 0x200000000 since
	 * this region is directly accessed elsewhere
	 * and remove them now is not gonna work. TODO: Map whathever is necessary
	 * into the kernel virtual space and unmap these regions.
	 */
	for (entry = 0; entry < 8; entry++) {
		if (entry == 0) {
			continue;
		}
		__asm__ volatile("idtlb %[idx]\n\t"
				"iitlb %[idx]\n\t"
				"dsync\n\t"
				"isync"
				:: [idx] "a"((entry << 29) | 6));
	}


	/* To finish, just restore vecbase and invalidate TLB entries
	 * used to map the relocated vecbase.
	 */
	__asm__ volatile("wsr.vecbase %0\n\t"
			:: "a"(vecbase));
	xtensa_dtlb_entry_invalidate(Z_XTENSA_TLB_ENTRY(Z_XTENSA_PTEVADDR + MB(4), 3));
	xtensa_itlb_entry_invalidate(Z_XTENSA_TLB_ENTRY(Z_XTENSA_PTEVADDR + MB(4), 3));
}

static bool l2_page_table_map(void *vaddr, uintptr_t phys, uint32_t flags)
{
	uint32_t l1_pos = (uint32_t)vaddr >> 22;
	uint32_t pte = Z_XTENSA_PTE(phys, 0, flags);
	uint32_t l2_pos = Z_XTENSA_L2_POS((uint32_t)vaddr);
	uint32_t *table;

	if (l1_page_table[l1_pos] == Z_XTENSA_MMU_ILLEGAL) {
		table  = alloc_l2_table();

		if (table == NULL) {
			return false;
		}

		l1_page_table[l1_pos] = Z_XTENSA_PTE((uint32_t)table, 0, Z_XTENSA_MMU_CACHED_WT);
	}

	table = (uint32_t *)(l1_page_table[l1_pos] & Z_XTENSA_PTE_PPN_MASK);
	table[l2_pos] = pte;

	xtensa_dtlb_autorefill_invalidate_sync(vaddr);
	xtensa_itlb_autorefill_invalidate_sync(vaddr);
	return true;
}

void arch_mem_map(void *virt, uintptr_t phys, size_t size, uint32_t flags)
{
	uint32_t va = (uint32_t)virt;
	uint32_t pa = (uint32_t)phys;
	uint32_t rem_size = (uint32_t)size;
	uint32_t xtensa_flags = 0;
	int key;

	if (size == 0) {
		LOG_ERR("Cannot map physical memory at 0x%08X: invalid "
			"zero size", (uint32_t)phys);
		k_panic();
	}

	switch (flags & K_MEM_CACHE_MASK) {

	case K_MEM_CACHE_WB:
		xtensa_flags |= Z_XTENSA_MMU_CACHED_WB;
		break;
	case K_MEM_CACHE_WT:
		xtensa_flags |= Z_XTENSA_MMU_CACHED_WT;
		break;
	case K_MEM_CACHE_NONE:
		__fallthrough;
	default:
		break;
	}

	if (flags & K_MEM_PERM_RW) {
		xtensa_flags |= Z_XTENSA_MMU_W;
	}
	if (flags & K_MEM_PERM_EXEC) {
		xtensa_flags |= Z_XTENSA_MMU_X;
	}

	key = arch_irq_lock();

	while (rem_size > 0) {
		bool ret = l2_page_table_map((void *)va, pa, xtensa_flags);

		__ASSERT(ret, "Virtual address (%u) already mapped", (uint32_t)virt);
		rem_size -= (rem_size >= KB(4)) ? KB(4) : rem_size;
		va += KB(4);
		pa += KB(4);
	}

	arch_irq_unlock(key);
}

static void l2_page_table_unmap(void *vaddr)
{
	uint32_t l1_pos = (uint32_t)vaddr >> 22;
	uint32_t l2_pos = Z_XTENSA_L2_POS((uint32_t)vaddr);
	uint32_t *table;
	uint32_t table_pos;

	if (l1_page_table[l1_pos] == Z_XTENSA_MMU_ILLEGAL) {
		return;
	}

	table = (uint32_t *)(l1_page_table[l1_pos] & Z_XTENSA_PTE_PPN_MASK);
	table[l2_pos] = Z_XTENSA_MMU_ILLEGAL;

	for (l2_pos = 0; l2_pos < XTENSA_L2_PAGE_TABLE_ENTRIES; l2_pos++) {
		if (table[l2_pos] != Z_XTENSA_MMU_ILLEGAL) {
			goto end;
		}
	}

	l1_page_table[l1_pos] = Z_XTENSA_MMU_ILLEGAL;
	table_pos = (table - (uint32_t *)l2_page_tables) / (XTENSA_L2_PAGE_TABLE_ENTRIES);
	atomic_clear_bit(l2_page_tables_track, table_pos);

end:
	xtensa_dtlb_autorefill_invalidate_sync(vaddr);
	xtensa_itlb_autorefill_invalidate_sync(vaddr);
}

void arch_mem_unmap(void *addr, size_t size)
{
	uint32_t va = (uint32_t)addr;
	uint32_t rem_size = (uint32_t)size;
	int key;

	if (addr == NULL) {
		LOG_ERR("Cannot unmap NULL pointer");
		return;
	}

	if (size == 0) {
		LOG_ERR("Cannot unmap virtual memory with zero size");
		return;
	}

	key = arch_irq_lock();

	while (rem_size > 0) {
		l2_page_table_unmap((void *)va);
		rem_size -= (rem_size >= KB(4)) ? KB(4) : rem_size;
		va += KB(4);
	}

	arch_irq_unlock(key);
}
