/*
 * Platform devices as ath10k's snoc.c uses them, in place of LinuxKPI's (whose platform_driver has no
 * probe): one device, the X01A's radio, made by lx10k_platform.c from what the kernel knows of it, and
 * one driver, snoc.c's, whose probe that file calls. module_platform_driver() leaves the driver where
 * lx10k_platform.c finds it (lx10k_platform_driver) instead of registering it at boot.
 */
#ifndef _LINUXKPI_LINUX_PLATFORM_DEVICE_H
#define _LINUXKPI_LINUX_PLATFORM_DEVICE_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/err.h>

struct fwnode_handle;

struct platform_device {
	const char *name;
	int id;
	bool id_auto;
	struct device dev;
	unsigned num_resources;
	struct resource *resource;
};

struct platform_driver {
	int (*probe)(struct platform_device *);
	void (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	struct device_driver driver;
};

struct platform_device_info {
	struct device *parent;
	struct fwnode_handle *fwnode;
	const char *name;
	int id;
	const struct resource *res;
	unsigned num_res;
	const void *data;
	size_t size_data;
	u64 dma_mask;
};

#define PLATFORM_DEVID_NONE	(-1)
#define dev_is_platform(dev)	(true)
#define to_platform_device(d)	container_of((d), struct platform_device, dev)

static inline void *platform_get_drvdata(const struct platform_device *pdev) { return (dev_get_drvdata(&pdev->dev)); }
static inline void platform_set_drvdata(struct platform_device *pdev, void *data) { dev_set_drvdata(&pdev->dev, data); }

struct resource *platform_get_resource_byname(struct platform_device *pdev, unsigned int type, const char *name);
struct resource *platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int index);
int platform_get_irq(struct platform_device *pdev, unsigned int index);
struct platform_device *platform_device_register_full(const struct platform_device_info *info);
void platform_device_unregister(struct platform_device *pdev);

extern struct platform_driver *lx10k_platform_driver;
#define module_platform_driver(drv)	struct platform_driver *lx10k_platform_driver = &(drv)

#endif
