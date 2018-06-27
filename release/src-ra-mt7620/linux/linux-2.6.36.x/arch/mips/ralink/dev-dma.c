#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/mach-ralink/rt_mmap.h>

static struct resource rt_dma_resource_dma[] = {
        {
                .start          = RALINK_GDMA_BASE,
                .end            = RALINK_GDMA_BASE + 0x7FF,
                .flags          = IORESOURCE_MEM,
        },
};

static struct platform_device rt_dma_dev = {
    .name = "rt_dma",
    .id   = 0,
        .num_resources  = ARRAY_SIZE(rt_dma_resource_dma),
        .resource               = rt_dma_resource_dma,
};


int __init mtk_dma_register(void)
{

	int retval = 0;

	retval = platform_device_register(&rt_dma_dev);
	if (retval != 0) {
		printk(KERN_ERR "register dma device fail\n");
		return retval;
	}


	return retval;
}
arch_initcall(mtk_dma_register);
