/*
 * LinuxKPI's iommu.h and what snoc.c adds to it: a domain of its own for the firmware's memory, made only
 * when the radio's node has a "wifi-firmware" child. The X01A's has none (its firmware lives in the
 * modem), so these are never reached; they say no.
 */
#ifndef _LX10K_LINUX_IOMMU_H
#define _LX10K_LINUX_IOMMU_H

#include_next <linux/iommu.h>
#include <linux/err.h>
#include <linux/errno.h>

#define IOMMU_READ	(1 << 0)
#define IOMMU_WRITE	(1 << 1)

static inline struct iommu_domain *iommu_paging_domain_alloc(struct device *dev) { return (ERR_PTR(-ENODEV)); }
static inline void iommu_domain_free(struct iommu_domain *domain) { }
static inline int iommu_attach_device(struct iommu_domain *domain, struct device *dev) { return (-ENODEV); }
static inline void iommu_detach_device(struct iommu_domain *domain, struct device *dev) { }
static inline int iommu_map(struct iommu_domain *domain, unsigned long iova, phys_addr_t paddr, size_t size, int prot,
    gfp_t gfp) { return (-ENODEV); }
static inline size_t iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size) { return (0); }

#endif
