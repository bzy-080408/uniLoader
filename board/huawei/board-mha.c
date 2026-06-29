/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Board driver for Huawei Mate 9
 *
 * Copyright (c) 2026, bzy <bzy-080408>
 */

#include <board.h>
#include <util.h>
#include <drivers/framework.h>
#include <lib/simplefb.h>
#include <lib/debug.h>

#ifdef CONFIG_EARLYCON
#define UART_BASE	0xFFF32000
#define UART_DR		(*(volatile uint32_t *)(UART_BASE + 0x000))
#define UART_FR		(*(volatile uint32_t *)(UART_BASE + 0x018))
#define UART_TXFF	(1 << 5)

void uart_putc(char ch)
{
	while (UART_FR & UART_TXFF)
		;
	UART_DR = ch;
}

void uart_puts(const char *s)
{
	while (*s)
		uart_putc(*s++);
}
#endif

/*
 * DSS (Display Subsystem) register access.
 *
 * The Kirin 960 DSS is at 0xE8600000. The AIF (AXI Interface) module
 * holds the framebuffer address, stride, and format for each channel.
 *
 * We read DSS ID register + AIF CH0 registers to detect the actual
 * framebuffer configuration left by the bootloader.
 *
 * Register layout (from Hi3660 DSS TRM / kernel driver):
 *   DSS base + 0x00000: DSS_RDMA_SEL (ID/version)
 *   AIF0 base = DSS base + 0x00006000 (typical for hi3660)
 *   AIF0_STRIDE   = AIF0 + 0x040  (stride in bytes)
 *   AIF0_DATA_ADDR0 = AIF0 + 0x080  (framebuffer address low 32 bits)
 */
#define DSS_BASE		0xE8600000UL
#define DSS_SIZE		0x80000

/* AIF module offset from DSS base */
#define DSS_AIF0_OFFSET		0x00006000
#define AIF_CH0_STRIDE		(DSS_BASE + DSS_AIF0_OFFSET + 0x040)
#define AIF_CH0_ADDR0		(DSS_BASE + DSS_AIF0_OFFSET + 0x080)
#define AIF_CH0_FMT		(DSS_BASE + DSS_AIF0_OFFSET + 0x030)

/* MIF (Memory InterFace) - contains the actual DMA read address */
#define DSS_MIF_OFFSET		0x00004000
#define MIF_CH0_ADDR		(DSS_BASE + DSS_MIF_OFFSET + 0x080)

/* DSS AXI - alternative location for fb address */
#define DSS_AXI_OFFSET		0x00005000
#define AXI_CH0_ADDR0		(DSS_BASE + DSS_AXI_OFFSET + 0x040)

static inline uint32_t dss_read(unsigned long addr)
{
	return *(volatile uint32_t *)addr;
}

static void dump_dss_regs(void)
{
	printk(KERN_INFO, "DSS: probing framebuffer from DSS registers...\n");

	/* Try multiple possible AIF/MIF/AXI register locations */
	unsigned long addrs[] = {
		AIF_CH0_ADDR0,
		AIF_CH0_STRIDE,
		AIF_CH0_FMT,
	};
	int n = ARRAY_SIZE(addrs);

	for (int i = 0; i < n; i++) {
		uint32_t val = dss_read(addrs[i]);
		printk(KERN_INFO, "  [0x%08x] = 0x%08x\n", addrs[i], val);
	}

	/* Scan AIF0 region for non-zero values that look like framebuffer addresses */
	printk(KERN_INFO, "DSS: scanning AIF0 region (0x%08x - 0x%08x)...\n",
	       DSS_BASE + DSS_AIF0_OFFSET, DSS_BASE + DSS_AIF0_OFFSET + 0x200);

	for (uint32_t off = 0; off < 0x200; off += 4) {
		uint32_t addr = DSS_BASE + DSS_AIF0_OFFSET + off;
		uint32_t val = dss_read(addr);
		/* Look for values that look like DDR addresses (0x00xxxxxx or 0x01xxxxxx) */
		if (val >= 0x00100000 && val < 0x90000000) {
			printk(KERN_INFO, "  [0x%08x + 0x%03x] = 0x%08x\n",
			       DSS_BASE + DSS_AIF0_OFFSET, off, val);
		}
	}
}

/* AO domain system control for UART6 clock */
#define SCTRL_BASE	0xFFF0A000
#define SCTRL_CLKEN_SET	(*(volatile uint32_t *)(SCTRL_BASE + 0x500))
#define SCTRL_CLKEN_DIS	(*(volatile uint32_t *)(SCTRL_BASE + 0x504))
#define SCTRL_CLKEN_STAT	(*(volatile uint32_t *)(SCTRL_BASE + 0x508))
#define UART6_CLK_BIT	0x09

static void uart6_clk_enable(void)
{
	SCTRL_CLKEN_SET = UART6_CLK_BIT;
	while ((SCTRL_CLKEN_STAT & UART6_CLK_BIT) != UART6_CLK_BIT)
		;
}

static struct video_info mha_fb = {
	.format = FB_FORMAT_ARGB8888,
	.width = 1080,
	.height = 1920,
	.stride = 4,
	.address = (void *)0x21000000,
};

static const struct device mha_devices[] = {
	{ "simplefb", &mha_fb, "fb" },
};

int mha_early_init(void)
{
	unsigned long cpacr;

	/* Enable FPU/SIMD */
	__asm__ volatile ("mrs %0, cpacr_el1" : "=r" (cpacr));
	cpacr |= (3UL << 20);
	__asm__ volatile ("msr cpacr_el1, %0" :: "r" (cpacr));

	/* Enable UART6 clock */
	uart6_clk_enable();

	/* Dump DSS registers to find the real framebuffer address */
	dump_dss_regs();

	return 0;
}

struct board_data board_ops = {
	.name = "huawei-mha",
	.ops = {
		.early_init = mha_early_init,
	},
	.devices = mha_devices,
	.num_devices = ARRAY_SIZE(mha_devices),
	.quirks = 0
};
