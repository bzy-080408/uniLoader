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

/* AO domain system control (sctrl) at 0xFFF0A000
 * Clock enable/disable/reset registers for peripherals in the AO domain.
 * From DTS: uart6 reset-controller-reg = <0x500 0x504 0x508 0x09>
 *   0x500: clock enable set register
 *   0x504: clock disable register
 *   0x508: clock status register
 *   0x09: bit mask for UART6 clock
 */
#define SCTRL_BASE	0xFFF0A000
#define SCTRL_CLKEN_SET	(*(volatile uint32_t *)(SCTRL_BASE + 0x500))
#define SCTRL_CLKEN_DIS	(*(volatile uint32_t *)(SCTRL_BASE + 0x504))
#define SCTRL_CLKEN_STAT	(*(volatile uint32_t *)(SCTRL_BASE + 0x508))
#define UART6_CLK_BIT	0x09

static void uart6_clk_enable(void)
{
	/* Enable UART6 clock */
	SCTRL_CLKEN_SET = UART6_CLK_BIT;

	/* Wait for clock to be enabled */
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
