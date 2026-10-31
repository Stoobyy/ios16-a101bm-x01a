/* What snoc.c reads of its device through the property interface (lx10k_platform.c answers). */
#ifndef _LX10K_LINUX_PROPERTY_H
#define _LX10K_LINUX_PROPERTY_H

#include <linux/types.h>

struct device;
const void *device_get_match_data(const struct device *dev);
int device_property_read_u32(const struct device *dev, const char *name, u32 *value);

#endif
