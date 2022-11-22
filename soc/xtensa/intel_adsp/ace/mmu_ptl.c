#include <inttypes.h>

#include <zephyr/arch/xtensa/xtensa_mmu.h>
#include <zephyr/linker/linker-defs.h>
#include <adsp_memory.h>

extern char _cached_start[];
extern char _cached_end[];
extern char _imr_start[];
extern char _imr_end[];
extern char __common_rom_region_start[];
extern char __common_rom_region_end[];
extern char __common_ram_region_start[];
extern char __common_ram_region_end[];
extern char __noinit_region_start[];
extern char __noinit_region_end[];

const struct xtensa_mmu_range xtensa_soc_mmu_ranges[] = {
	{
		.start = (uint32_t)__common_rom_region_start,
		.end   = (uint32_t)__common_rom_region_end,
		.name = "common-rom",
	},
	{
		.start = (uint32_t)__common_ram_region_start,
		.end   = (uint32_t)__common_ram_region_end,
		.attrs = Z_XTENSA_MMU_W,
		.name = "common-ram",
	},
	{
		.start = (uint32_t)__noinit_region_start,
		.end   = (uint32_t)__noinit_region_end,
		.attrs = Z_XTENSA_MMU_W,
		.name = "noinit",
	},
	{
		.start = (uint32_t)VECBASE_RESET_PADDR_SRAM,
		.end   = (uint32_t)VECBASE_RESET_PADDR_SRAM + VECTOR_TBL_SIZE,
		.attrs = Z_XTENSA_MMU_X,
		.name = "exceptions",
	},
	{
		.start = (uint32_t)_cached_start,
		.end   = (uint32_t)_cached_end,
		.attrs = Z_XTENSA_MMU_X | Z_XTENSA_MMU_W | Z_XTENSA_MMU_CACHED_WB,
		.name = "cached",
	},
	{
		.start = (uint32_t)HP_SRAM_WIN0_BASE,
		.end   = (uint32_t)HP_SRAM_WIN0_BASE + (uint32_t)HP_SRAM_WIN0_SIZE,
		.attrs = Z_XTENSA_MMU_W | Z_XTENSA_MMU_MAP_UNCACHED,
		.name = "win0",
	},
	{
		.start = (uint32_t)HP_SRAM_WIN1_BASE,
		.end   = (uint32_t)HP_SRAM_WIN1_BASE + (uint32_t)HP_SRAM_WIN1_SIZE,
		.attrs = Z_XTENSA_MMU_W,
		.name = "win2",
	},
	{
		.start = (uint32_t)HP_SRAM_WIN2_BASE,
		.end   = (uint32_t)HP_SRAM_WIN2_BASE + (uint32_t)HP_SRAM_WIN2_SIZE,
		.attrs = Z_XTENSA_MMU_W | Z_XTENSA_MMU_MAP_UNCACHED,
		.name = "win2",
	},
	{
		.start = (uint32_t)HP_SRAM_WIN3_BASE,
		.end   = (uint32_t)HP_SRAM_WIN3_BASE + (uint32_t)HP_SRAM_WIN3_SIZE,
		.attrs = Z_XTENSA_MMU_W | Z_XTENSA_MMU_MAP_UNCACHED,
		.name = "win3",
	},
	{
		.start = (uint32_t)z_mapped_start,
		.end   = (uint32_t)IMR_BOOT_LDR_MANIFEST_BASE,
		.attrs = Z_XTENSA_MMU_W | Z_XTENSA_MMU_X | Z_XTENSA_MMU_CACHED_WB,
		.name = "stack",
	},
	{
		.start = (uint32_t)IMR_BOOT_LDR_MANIFEST_BASE,
		.end   = (uint32_t)_imr_end,
		.attrs = Z_XTENSA_MMU_X | Z_XTENSA_MMU_W,
		.name = "imr",
	},
	{
		.start = (uint32_t)LP_SRAM_BASE,
		.end   = (uint32_t)(LP_SRAM_BASE + LP_SRAM_SIZE),
		.attrs = Z_XTENSA_MMU_W | Z_XTENSA_MMU_CACHED_WB,
		.name = "lpsram",
	},
};

int xtensa_soc_mmu_ranges_num = ARRAY_SIZE(xtensa_soc_mmu_ranges);
