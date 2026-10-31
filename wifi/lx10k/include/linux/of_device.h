/* (of_dma_configure(): the radio's DMA is set up by the kernel, not by the device tree) */
#ifndef _LX10K_LINUX_OF_DEVICE_H
#define _LX10K_LINUX_OF_DEVICE_H

#include <linux/of.h>

struct device;
static inline int of_dma_configure(struct device *dev, struct device_node *np, bool force) { return (0); }

#endif
