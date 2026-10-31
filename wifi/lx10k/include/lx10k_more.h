/*
 * What ath10k uses of Linux and neither LinuxKPI (FreeBSD 15.1) nor HakiOS's wifi/lx declares. Read
 * after wifi/lx's prelude, before every file of the driver. Small things each.
 */
#ifndef _LX10K_MORE_H_
#define _LX10K_MORE_H_

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/netdevice.h>

/* --- guard(spinlock_bh), as Linux's spinlock.h defines it */
DEFINE_LOCK_GUARD_1(spinlock_bh, spinlock_t, spin_lock_bh(_T->lock), spin_unlock_bh(_T->lock))

/* --- calibration from NVMEM cells: the phone has none for the radio */
struct nvmem_cell;
static inline struct nvmem_cell *devm_nvmem_cell_get(struct device *dev, const char *id) { return (ERR_PTR(-ENOENT)); }
static inline void *nvmem_cell_read(struct nvmem_cell *cell, size_t *len) { return (ERR_PTR(-ENOENT)); }

/*
 * --- the device tree: what core.c and mac.c read of the radio's node besides what wifi/lx declares.
 * The X01A's node (qcom,icnss) has none of these: calibration data, coexistence, an external
 * front-end's name.
 */
static inline int
of_property_read_u8_array(const struct device_node *np, const char *name, u8 *out, size_t size)
{
	return (-EINVAL);
}

static inline int
of_property_read_u8(const struct device_node *np, const char *name, u8 *out)
{
	return (-EINVAL);
}

static inline int
of_property_read_string_index(const struct device_node *np, const char *name, int index, const char **out)
{
	return (-EINVAL);
}

/* --- interrupts: snoc.c asks for its copy engines' lines to be left off until it enables them */
#ifndef IRQF_NO_AUTOEN
#define IRQF_NO_AUTOEN		0x00080000
#endif

/* --- NAPI in a thread of its own: the X01A's poller is lx10k_platform.c's, whatever the driver asks */
static inline int netif_threaded_enable(struct net_device *dev) { return (0); }

/* --- memory that is not RAM of ours (the firmware's MSA, for a dump only): mapped as a device's */
#define MEMREMAP_WB		(1 << 0)
#define MEMREMAP_WT		(1 << 1)
#define MEMREMAP_WC		(1 << 2)
static inline void *
devm_memremap(struct device *dev, resource_size_t offset, size_t size, unsigned long flags)
{
	void *mapped = ioremap(offset, size);

	return (mapped != NULL ? mapped : ERR_PTR(-ENOMEM));
}

#endif
