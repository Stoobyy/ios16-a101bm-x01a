/*
 * The X01A's platform under ath10k: the one device that snoc.c probes ("qcom,wcn3990-wifi"), and the
 * bit of Linux's platform machinery it asks for on the way. What the kernel already does is not
 * redone here: the five supplies are switched on before the driver starts (bsd/dev/hakios.wcn3990.c,
 * power_on()), the modem runs before the driver and is not restarted while it runs (so no SSR ever
 * comes), and every byte the radio reads or writes comes out of wifi/lx's pool (lx_base.c). The
 * copy engines' lines are not GIC SPIs yet: they are twelve virtual numbers, watched by a thread
 * that calls their handlers once a millisecond, like lx_bus.c's "rings looked at" mode with no IRQ.
 *
 * The entries are here, as lx_bus.c has for ath11k: lx_prepare, lx_start, lx_stop, lx_transmit.
 * lx_start() probes the driver on the device and waits until core.c has registered (60 s at most);
 * the station -- scan, join, frames -- uses the shared HakiOS lx_80211.c layer.
 * The tested X01A mode auto-joins, then publishes the iOS station callbacks.
 */
#define LX_QMI_AS_IT_IS		/* (the driver's requests to the firmware go past untouched) */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/notifier.h>
#include <linux/ioport.h>
#include <linux/pci.h>			/* IORESOURCE_MEM */
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>

#include <lx_os.h>
#include "lx.h"
#include "core.h"			/* struct ath10k, dev_flags */
#include "snoc.h"			/* ath10k_snoc_priv(), the snoc's own flags */
#include "hif.h"
#include "ce.h"
#include "debug.h"			/* ath10k_debug_mask, ATH10K_DBG_* */

/* ---- the radio's node ------------------------------------------------------------------------------ */

/*
 * What the driver reads of its node (quirks_init, qmi.c's MSA questions), and no more: no
 * "qcom,snoc-host-cap-8bit-quirk", no "firmware-name", no "qcom,no-msa-ready-indicator",
 * no "wifi-firmware" child (the firmware lives in the modem), no xo-cal-data.
 */
static const char radio_msa_value[1] = "";

static const struct property radio_properties[] = {
	{ "qcom,msa-fixed-perm", 0, radio_msa_value },
};

static struct device_node radio_node = {
	.name = "wifi",
	.full_name = "wcn3990-wifi",
	.nproperties = nitems(radio_properties),
	.properties = radio_properties,
};

const struct property *
of_find_property(const struct device_node *np, const char *name, int *lenp)
{
	if (np == NULL) {
		return (NULL);
	}
	for (int i = 0; i < np->nproperties; i++) {
		if (strcmp(np->properties[i].name, name) == 0) {
			if (lenp != NULL) {
				*lenp = np->properties[i].length;
			}
			return (&np->properties[i]);
		}
	}
	return (NULL);
}

struct device_node *
of_get_child_by_name(const struct device_node *np, const char *name)
{
	/* (the radio has no "wifi-firmware" child: snoc.c then keeps its TrustZone way) */
	return (NULL);
}

/*
 * The firmware's MSA: the piece the X01A's device tree calls pil_wlan_fw_region, 2 MB, from which
 * the WCN3990's firmware runs. TrustZone refuses every hyp-assign of it (-2, 0x56), and Android's
 * icnss makes none either, which is what "qcom,msa-fixed-perm" above says.
 */
#define LX10K_MSA_PA		0x8ba00000ull
#define LX10K_MSA_SIZE		0x200000ull

int
of_reserved_mem_region_to_resource(const struct device_node *np, unsigned int index, struct resource *res)
{
	if (np != &radio_node || index != 0) {
		return (-ENODEV);
	}
	res->start = LX10K_MSA_PA;
	res->end = LX10K_MSA_PA + LX10K_MSA_SIZE - 1;
	res->flags = IORESOURCE_MEM;
	return (0);
}

/* ---- the device: one, made for snoc.c's probe ------------------------------------------------------- */

/* membase: the copy engines' registers; from the X01A's icnss node */
#define LX10K_MEM_PA		0x18800000ull
#define LX10K_MEM_SIZE		0x800000ull
#define LX10K_CE_COUNT		12

static struct resource radio_resources[] = {
	{ LX10K_MEM_PA, LX10K_MEM_PA + LX10K_MEM_SIZE - 1, "membase", IORESOURCE_MEM },
};

/* the twelve SPIs of the copy engines, CE0 to CE11, in the X01A's device tree (for the kernel's
 * wiring of real interrupts later; the polled numbers below are this array's index plus one): */
static const int radio_ce_spis[LX10K_CE_COUNT] = { 132, 81, 415, 416, 417, 86, 94, 96, 82, 119, 120, 87 };

struct resource *
platform_get_resource_byname(struct platform_device *pdev, unsigned int type, const char *name)
{
	for (unsigned i = 0; i < pdev->num_resources; i++) {
		if (pdev->resource[i].flags == type && pdev->resource[i].name != NULL &&
		    strcmp(pdev->resource[i].name, name) == 0) {
			return (&pdev->resource[i]);
		}
	}
	return (NULL);
}

struct resource *
platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int index)
{
	unsigned n = 0;

	for (unsigned i = 0; i < pdev->num_resources; i++) {
		if (pdev->resource[i].flags == type && n++ == index) {
			return (&pdev->resource[i]);
		}
	}
	return (NULL);
}

int
platform_get_irq(struct platform_device *pdev, unsigned int index)
{
	if (index >= LX10K_CE_COUNT) {
		return (-ENXIO);
	}
	return ((int)index + 1);		/* virtual: the polled table below is keyed by it */
}

struct platform_device *
platform_device_register_full(const struct platform_device_info *info)
{
	/* (only reached for a "wifi-firmware" child, of which there is none) */
	return (ERR_PTR(-ENODEV));
}

void
platform_device_unregister(struct platform_device *pdev)
{
}

/* ---- supplies: the kernel has them on already ------------------------------------------------------- */

static uint8_t regulator_object;

int
devm_regulator_bulk_get(struct device *dev, int count, struct regulator_bulk_data *supplies)
{
	for (int i = 0; i < count; i++) {
		supplies[i].consumer = (struct regulator *)&regulator_object;
		supplies[i].ret = 0;
	}
	return (0);
}

int
regulator_bulk_enable(int count, struct regulator_bulk_data *supplies)
{
	for (int i = 0; i < count; i++) {
		supplies[i].ret = 0;
	}
	return (0);
}

int
regulator_bulk_disable(int count, struct regulator_bulk_data *supplies)
{
	for (int i = 0; i < count; i++) {
		supplies[i].ret = 0;
	}
	return (0);
}

/* ---- the copy engines' lines: virtual numbers, called from a thread every 1 ms ---------------------- */

static struct {
	irq_handler_t handler;
	void *arg;
	bool requested;
	bool enabled;
	unsigned active;
} ce_irqs[1 + LX10K_CE_COUNT];

static unsigned ce_poll_ready;
static unsigned ce_poll_active;
static void *ce_poll_thread;
void
lx10k_ce_poll_ready(int ready)
{
	__atomic_store_n(&ce_poll_ready, ready != 0, __ATOMIC_SEQ_CST);
	if (!ready && lx_os_thread_self() != __atomic_load_n(&ce_poll_thread, __ATOMIC_ACQUIRE))
		while (__atomic_load_n(&ce_poll_active, __ATOMIC_SEQ_CST)) lx_pause_ms(1);
}

static void
ce_polls(void *unused)
{
	__atomic_store_n(&ce_poll_thread, lx_os_thread_self(), __ATOMIC_RELEASE);
	for (;;) {
		if (!__atomic_load_n(&ce_poll_ready, __ATOMIC_ACQUIRE)) {
			lx_pause_ms(1);
			continue;
		}
		__atomic_store_n(&ce_poll_active, 1, __ATOMIC_SEQ_CST);
		if (!__atomic_load_n(&ce_poll_ready, __ATOMIC_SEQ_CST)) {
			__atomic_store_n(&ce_poll_active, 0, __ATOMIC_SEQ_CST);
			continue;
		}
		for (unsigned v = 1; v < nitems(ce_irqs); v++) {
			if (__atomic_load_n(&ce_irqs[v].requested, __ATOMIC_ACQUIRE) &&
			    __atomic_load_n(&ce_irqs[v].enabled, __ATOMIC_SEQ_CST)) {
				__atomic_store_n(&ce_irqs[v].active, 1, __ATOMIC_SEQ_CST);
				if (!__atomic_load_n(&ce_irqs[v].enabled, __ATOMIC_SEQ_CST)) {
					__atomic_store_n(&ce_irqs[v].active, 0, __ATOMIC_SEQ_CST);
					continue;
				}
				struct ath10k *ar = ce_irqs[v].arg;
				/* Poll pending hardware IRQs, not an invented interrupt on every CE. */
				if (ath10k_ce_interrupt_summary(ar) & (1u << (v - 1)))
					ce_irqs[v].handler((int)v, ar);
				__atomic_store_n(&ce_irqs[v].active, 0, __ATOMIC_SEQ_CST);
			}
		}
		__atomic_store_n(&ce_poll_active, 0, __ATOMIC_SEQ_CST);
		lx_pause_ms(1);
	}
}

static int irq_thread_begun;

static int
ce_irqs_begin(void)
{
	if (irq_thread_begun) {
		return (0);
	}
	if (lx_os_thread("wifi-ce", ce_polls, NULL) != 0) {
		return (-ENOMEM);
	}
	irq_thread_begun = true;
	return (0);
}

int
lkpi_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, irq_handler_t thread_handler,
    unsigned long flags, const char *name, void *arg)
{
	if (irq == 0 || irq >= nitems(ce_irqs) || handler == NULL) {
		return (-EINVAL);
	}
	ce_irqs[irq].handler = handler;
	ce_irqs[irq].arg = arg;
	__atomic_store_n(&ce_irqs[irq].requested, true, __ATOMIC_RELEASE);
	__atomic_store_n(&ce_irqs[irq].enabled, (flags & IRQF_NO_AUTOEN) == 0, __ATOMIC_SEQ_CST);
	return (0);
}

int
lkpi_enable_irq(unsigned int irq)
{
	if (irq == 0 || irq >= nitems(ce_irqs)) {
		return (-EINVAL);
	}
	__atomic_store_n(&ce_irqs[irq].enabled, true, __ATOMIC_SEQ_CST);
	return (0);
}

void
lkpi_disable_irq(unsigned int irq)
{
	if (irq < nitems(ce_irqs)) {
		__atomic_store_n(&ce_irqs[irq].enabled, false, __ATOMIC_SEQ_CST);
		if (lx_os_thread_self() != __atomic_load_n(&ce_poll_thread, __ATOMIC_ACQUIRE))
			while (__atomic_load_n(&ce_irqs[irq].active, __ATOMIC_SEQ_CST)) lx_pause_ms(1);
	}
}

void
lkpi_free_irq(unsigned int irq, void *arg)
{
	if (irq < nitems(ce_irqs) && (arg == NULL || ce_irqs[irq].arg == arg)) {
		lkpi_disable_irq(irq);
		__atomic_store_n(&ce_irqs[irq].requested, false, __ATOMIC_RELEASE);
		ce_irqs[irq].handler = NULL;
		ce_irqs[irq].arg = NULL;
		__atomic_store_n(&ce_irqs[irq].enabled, false, __ATOMIC_SEQ_CST);
	}
}

/* ---- NAPI: the polls run where napi_schedule() is called ------------------------------------------- */

/* bits 0 to 3 are LinuxKPI's (sched, missing/missed, task, shutdown); ours comes after them */
#define LX10K_NAPI_DISABLED	(1ul << 4)

void
linuxkpi_netif_napi_add(struct net_device *dev, struct napi_struct *napi, int (*poll)(struct napi_struct *, int))
{
	memset(napi, 0, sizeof(*napi));
	INIT_LIST_HEAD(&napi->rx_list);
	napi->dev = dev;
	napi->poll = poll;
	napi->budget = 64;
}

void
linuxkpi_netif_napi_del(struct napi_struct *napi)
{
	memset(napi, 0, sizeof(*napi));
}

bool
linuxkpi_napi_schedule(struct napi_struct *napi)
{
	if (napi->state & LX10K_NAPI_DISABLED) {
		return (false);
	}
	(void)napi->poll(napi, napi->budget);	/* napi_complete() within clears LinuxKPI's bits */
	return (true);
}

bool
linuxkpi_napi_complete(struct napi_struct *napi)
{
	napi->state &= ~0xful;
	return (true);
}

void
linuxkpi_napi_enable(struct napi_struct *napi)
{
	napi->state &= ~LX10K_NAPI_DISABLED;
}

void
linuxkpi_napi_disable(struct napi_struct *napi)
{
	napi->state |= LX10K_NAPI_DISABLED;
}

void
linuxkpi_napi_synchronize(struct napi_struct *napi)
{
	/* (inline polls: nothing runs behind the caller's back) */
}

/* ---- memory: tags ask nothing, and the pool answers the coherent allocations ------------------------ */

int
linux_dma_tag_init(struct device *dev, u64 mask)
{
	/* (dma_set_mask() insisted on a tag beforehand: the pool asks nothing of tags) */
	return (dev->dma_priv != NULL ? 0 : -EIO);
}

int
linux_dma_tag_init_coherent(struct device *dev, u64 mask)
{
	return (linux_dma_tag_init(dev, mask));
}

void *
linuxkpi_dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
	return (linux_dma_alloc_coherent(dev, size, dma_handle, flag));
}

/* ---- sleep in sbintime's 32.32 seconds -------------------------------------------------------------- */

int
pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t precision, int flags)
{
	unsigned long long ns = (((unsigned long long)sbt >> 32) * 1000000000ull +
	    (((unsigned long long)sbt & 0xffffffffull) * 1000000000ull >> 32)) +
	    (((unsigned long long)precision >> 32) * 1000000000ull +
	    (((unsigned long long)precision & 0xffffffffull) * 1000000000ull >> 32));
	unsigned long long until = lx_os_nanoseconds() + ns;

	while (lx_os_nanoseconds() < until) {
	}
	return (0);
}

/* ---- the modem: up before the driver, and not restarted while it runs (so no SSR, and no SCM move) -- */

static uint8_t qcom_ssr_object;

void *
qcom_register_ssr_notifier(const char *name, struct notifier_block *nb)
{
	return (&qcom_ssr_object);
}

int
qcom_unregister_ssr_notifier(void *notify, struct notifier_block *nb)
{
	return (notify == &qcom_ssr_object ? 0 : -EINVAL);
}

int
qcom_scm_assign_mem(phys_addr_t address, size_t size, u64 *source, const struct qcom_scm_vmperm *next,
    unsigned int count)
{
	/* (the node says "qcom,msa-fixed-perm", and every caller of this checks it first) */
	return (0);
}

/* ---- properties read the other Linux way ------------------------------------------------------------ */

const void *
device_get_match_data(const struct device *dev)
{
	const struct of_device_id *table = lx10k_platform_driver->driver.of_match_table;

	if (table == NULL) {
		return (NULL);
	}
	return (table[0].data);
}

int
device_property_read_u32(const struct device *dev, const char *name, u32 *value)
{
	/* (the radio has no xo-cal-data) */
	return (-ENOENT);
}

/* ---- the driver's requests to the firmware go past untouched ---------------------------------------- */

int
lx_qmi_send_request(struct qmi_handle *qmi, struct sockaddr_qrtr *sq, struct qmi_txn *txn, int msg_id, size_t largest,
    const struct qmi_elem_info *ei, const void *structure)
{
	return (qmi_send_request(qmi, sq, txn, msg_id, largest, ei, structure));
}

/* ---- the entries ------------------------------------------------------------------------------------- */

static struct device radio_dev;
static struct platform_device radio_pdev;
void lx_stop(void);
static struct ath10k *volatile driver;	/* the driver, while it is up: between lx_start() and lx_stop() */

/* The thread that counts the ticks, and the memory the radio is shown: once. 0, or an errno. */
int
lx_prepare(void)
{
	return (lx_base_init() != 0 || lx_pool_init() != 0 || ce_irqs_begin() != 0 ? ENOMEM : 0);
}

int
lx_start(void)
{
	unsigned long long began;
	struct ath10k *ar;
	int ret;

	if (lx_prepare() != 0) {
		lx_tell("lx: no thread, or no memory the radio can be given\n");
		return (ENOMEM);
	}
	if (lx_os_option("wifi_quiet")) {
		ath10k_debug_mask = 0;
	} else {
		ath10k_debug_mask = ATH10K_DBG_QMI | ATH10K_DBG_BOOT | ATH10K_DBG_HTC | ATH10K_DBG_SNOC | ATH10K_DBG_BMI;
	}

	memset(&radio_dev, 0, sizeof(radio_dev));
	radio_dev.kobj.name = "wifi";
	radio_dev.of_node = &radio_node;
	radio_dev.dma_priv = (void *)1;		/* dma_set_mask() insists on a tag beforehand */
	memset(&radio_pdev, 0, sizeof(radio_pdev));
	radio_pdev.name = "wcn3990-wifi";
	radio_pdev.id = PLATFORM_DEVID_NONE;
	radio_pdev.dev = radio_dev;
	radio_pdev.num_resources = nitems(radio_resources);
	radio_pdev.resource = radio_resources;

	ath10k_frame_mode = ATH10K_HW_TXRX_ETHERNET;

	/* probe synchronously, as snoc.c's own module_platform_driver would */
	if ((ret = lx10k_platform_driver->probe(&radio_pdev)) != 0) {
		lx_tell("lx: ath10k's probe says %d\n", ret);
		return (-ret);
	}
	ar = platform_get_drvdata(&radio_pdev);
	if (ar == NULL) {
		lx_tell("lx: ath10k attached nothing to the device\n");
		return (EIO);
	}

	driver = ar; /* retain ownership even if asynchronous startup fails */

	/*
	 * The rest happens on the driver's own threads: QMI with the modem, BDF and calibration out
	 * of image/, firmware-5.bin, then core_start() and htc_wait_target(). It went well if core.c
	 * has registered (its flag) or has said on its way that it gives up.
	 */
	began = lx_os_nanoseconds();
	while (!test_bit(ATH10K_FLAG_CORE_REGISTERED, &ar->dev_flags) &&
	    !test_bit(ATH10K_SNOC_FLAG_UNREGISTERING, &((struct ath10k_snoc *)ath10k_snoc_priv(ar))->flags) &&
	    lx_os_nanoseconds() - began < 60000000000ull) {
		lx_pause_ms(10);
	}
	lx_tell("lx: %llu ms after ath10k's start the driver %s; its flags: %#lx\n",
	    (lx_os_nanoseconds() - began) / 1000000ull,
	    test_bit(ATH10K_FLAG_CORE_REGISTERED, &ar->dev_flags) ?
	    "IS UP as far as it is built: core.c has registered it" :
	    "HAS NOT GOT THROUGH its start",
	    ar->dev_flags);
	if (!test_bit(ATH10K_FLAG_CORE_REGISTERED, &ar->dev_flags)) {
		lx_stop();
		return (ETIMEDOUT);
	}
	driver = ar;

	if (lx_os_option("wifi_station")) {
        /* Optional known-network bootstrap before publishing iOS callbacks. */
        if (lx_os_option("wifi_join")) {
            for (unsigned attempt = 0; attempt < 3; attempt++) {
                ret = lx_station_join();
                if (ret != ETIMEDOUT) break;
                printf("lx10k: bootstrap join attempt %u timed out%s\n", attempt + 1,
                    attempt < 2 ? "; retrying" : "; stopping");
                if (attempt < 2) msleep(1000);
            }
            if (ret != 0) {
                lx_stop();
                return ret;
            }
        }
		ret = lx_station_offer();
	} else if (lx_os_option("wifi_join")) {
		ret = lx_station_join();
		if (ret == 0) return 0;
	} else {
		ret = lx_os_option("wifi_scan") ? lx_station_scan() : 0;
	}
	if (ret == 0 && lx_os_option("wifi_station")) return 0;
	lx_stop();
	return ret;
}

/* The driver down, as its own removal does it (the copy engines are let go of on the way). */
void
lx_stop(void)
{
	struct ath10k *ar = driver;

	if (ar == NULL) {
		return;
	}
	driver = NULL;
	lx10k_ce_poll_ready(0); /* wait for in-flight CE callback before teardown */
	lx_station_withdraw();
	lx_station_leave();
	lx10k_platform_driver->remove(&radio_pdev);
	memset(&radio_pdev, 0, sizeof(radio_pdev));
	lx_tell("lx: the driver is down again\n");
}

/* Pass Ethernet frames through the active shared station. */
int
lx_transmit(const unsigned char *frame, unsigned long length)
{
	return (driver != NULL ? lx_station_send(frame, length) : ENETDOWN);
}
