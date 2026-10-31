/*
 * What ath10k asks of the layer under it and HakiOS's wifi/lx does not have, because ath11k did not
 * ask: a few of FreeBSD's kernel functions, a few of LinuxKPI's, and the small part of the device
 * tree that wifi/lx's lx_bus.c answered for ath11k (that file is not built here: lx10k_bus.c is the
 * X01A's bus).
 *
 * mac80211's queues of frames per station and TID (ieee80211_txq) are not run by wifi/lx: its
 * lx_80211.c hands every frame to the driver's tx() as it comes. So the scheduler's calls below
 * answer as mac80211 does when no queue holds a frame.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>

#include <lx_os.h>
#include "lx.h"

/* ---- FreeBSD's kernel ----------------------------------------------------------------------------- */

void
DELAY(int usec)
{
	unsigned long long until = lx_os_nanoseconds() + (unsigned long long)usec * 1000;

	while (lx_os_nanoseconds() < until) {
	}
}

void
nanouptime(struct timespec *tsp)
{
	unsigned long long now = lx_os_nanoseconds();

	tsp->tv_sec = now / 1000000000ull;
	tsp->tv_nsec = now % 1000000000ull;
}

/* every event may be said: the log is the kernel's to keep short */
int
eventratecheck(struct timeval *last, int *count, int most)
{
	return (1);
}

int
device_printf(device_t dev, const char *format, ...)
{
	char line[256];
	va_list list;
	int length;

	va_start(list, format);
	length = vsnprintf(line, sizeof(line), format, list);
	va_end(list);
	lx_os_print(line);
	return (length);
}

/*
 * A mutex made by MTX_SYSINIT is initialised from a linker set at boot, and there is none here; a
 * zeroed lock of wifi/lx is a lock that nobody holds, which is what such a mutex is before it is used.
 */
void
mtx_sysinit(const void *arg)
{
}

/* CRC-32 (IEEE 802.3, reflected), the table FreeBSD's libkern has; made the first time it is wanted */
uint32_t crc32_tab[256];

static void __attribute__((constructor))
crc32_tab_make(void)
{
	for (uint32_t n = 0; n < 256; n++) {
		uint32_t c = n;

		for (int k = 0; k < 8; k++) {
			c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
		}
		crc32_tab[n] = c;
	}
}

/* ---- devres: memory that goes with a device. The radio's device is never taken away here. -------- */

void *
lkpi_devres_alloc(void (*release)(struct device *, void *), size_t size, gfp_t gfp)
{
	return (kzalloc(size, gfp));
}

void
lkpi_devres_add(struct device *dev, void *resource)
{
}

void
lkpi_devm_kmalloc_release(struct device *dev, void *resource)
{
}

/* ---- buffers and network devices ------------------------------------------------------------------ */

struct sk_buff *
linuxkpi_skb_copy(const struct sk_buff *skb, gfp_t gfp)
{
	struct sk_buff *copy = dev_alloc_skb(skb->len);

	if (copy != NULL) {
		memcpy(skb_put(copy, skb->len), skb->data, skb->len);
		copy->priority = skb->priority;
		copy->protocol = skb->protocol;
		memcpy(copy->cb, skb->cb, sizeof(copy->cb));
	}
	return (copy);
}

/* ath10k's one network device is a dummy for its NAPI context: memory, and its private part */
struct net_device *
linuxkpi_alloc_netdev(size_t private, const char *name, uint32_t assign, void (*setup)(struct net_device *))
{
	struct net_device *dev = kzalloc(sizeof(*dev) + private, GFP_KERNEL);

	if (dev != NULL) {
		strlcpy(dev->name, name, sizeof(dev->name));
		if (setup != NULL) {
			setup(dev);
		}
	}
	return (dev);
}

void
linuxkpi_free_netdev(struct net_device *dev)
{
	kfree(dev);
}

/* (NAPI is lx10k_platform.c's: it runs the radio's polls) */

/* ---- 802.11 ---------------------------------------------------------------------------------------- */

const uint8_t rfc1042_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

/* the TID of a QoS data frame (IEEE 802.11-2020, 9.2.4.5.2); 0 for anything else */
uint8_t
linuxkpi_ieee80211_get_tid(struct ieee80211_hdr *hdr, bool nonqos_ok)
{
	if (!ieee80211_is_data_qos(hdr->frame_control)) {
		return (0);
	}
	return (*ieee80211_get_qos_ctl(hdr) & IEEE80211_QOS_CTL_TID_MASK);
}

/* in 100 kb/s, as cfg80211 has it; a legacy rate only (the rest is for statistics nobody reads here) */
uint32_t
linuxkpi_cfg80211_calculate_bitrate(struct rate_info *rate)
{
	return (rate->legacy);
}

struct linuxkpi_ieee80211_regdomain *
lkpi_get_linuxkpi_ieee80211_regdomain(size_t rules)
{
	return (kzalloc(sizeof(struct linuxkpi_ieee80211_regdomain) + rules * sizeof(struct ieee80211_reg_rule), GFP_KERNEL));
}

/* mac80211's per-station queues, as when none holds a frame (see the top of this file) */
void
linuxkpi_ieee80211_txq_schedule_start(struct ieee80211_hw *hw, uint8_t ac)
{
}

struct ieee80211_txq *
linuxkpi_ieee80211_next_txq(struct ieee80211_hw *hw, uint8_t ac)
{
	return (NULL);
}

void
linuxkpi_ieee80211_schedule_txq(struct ieee80211_hw *hw, struct ieee80211_txq *txq, bool withoutpkts)
{
}

struct sk_buff *
linuxkpi_ieee80211_tx_dequeue(struct ieee80211_hw *hw, struct ieee80211_txq *txq)
{
	return (NULL);
}

void
linuxkpi_ieee80211_txq_get_depth(struct ieee80211_txq *txq, unsigned long *frames, unsigned long *bytes)
{
	if (frames != NULL) {
		*frames = 0;
	}
	if (bytes != NULL) {
		*bytes = 0;
	}
}

/* one access category's queue: wifi/lx stops and wakes all of them together */
void
linuxkpi_ieee80211_stop_queue(struct ieee80211_hw *hw, int queue)
{
	ieee80211_stop_queues(hw);
}

void
linuxkpi_ieee80211_wake_queue(struct ieee80211_hw *hw, int queue)
{
	ieee80211_wake_queues(hw);
}

/* ---- the device tree, of which there is none: the X01A's radio is described by lx10k_bus.c -------- */

const void *
of_get_property(const struct device_node *np, const char *name, int *lenp)
{
	return (NULL);
}

int
of_property_read_string(const struct device_node *np, const char *name, const char **out)
{
	return (-EINVAL);
}

/* the radio's address, if the kernel has one for it (lx_os_radio_address()) */
int
linuxkpi_device_get_mac_address(struct device *dev, char *address)
{
	return (lx_os_radio_address((unsigned char *)address) ? 0 : -ENOENT);
}
