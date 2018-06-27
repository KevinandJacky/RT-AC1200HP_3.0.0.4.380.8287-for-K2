/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by Ralf Baechle
 */
#include <linux/clocksource.h>
#include <linux/init.h>

#include <asm/time.h>

static cycle_t c0_hpt_read(struct clocksource *cs)
{
	return read_c0_count();
}

static struct clocksource clocksource_mips = {
	.name		= "MIPS",
	.mask		= CLOCKSOURCE_MASK(32),
	.read		= c0_hpt_read,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

int __init init_r4k_clocksource(void)
{
#if defined(CONFIG_RALINK_MT7620) && defined(CONFIG_MTD_SPI_RALINK)
	printk("%s: cpu_has_counter(%ld) mips_hpt_frequency(%d)\n", __func__, cpu_has_counter, mips_hpt_frequency);
	mips_hpt_frequency -= 739690;   //9min 39sec / 2day 15hour 3min 16sec
	printk("new mips_hpt_frequency(%d)\n", mips_hpt_frequency);
#endif

	if (!cpu_has_counter || !mips_hpt_frequency)
		return -ENXIO;

	/* Calculate a somewhat reasonable rating value */
	clocksource_mips.rating = 200 + mips_hpt_frequency / 10000000;
	clocksource_set_clock(&clocksource_mips, mips_hpt_frequency);
	clocksource_register(&clocksource_mips);

	return 0;
}
