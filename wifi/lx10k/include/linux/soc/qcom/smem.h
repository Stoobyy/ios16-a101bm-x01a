/*
 * Shared memory, which qmi.c writes the firmware's build into (the image version table, item 469).
 * The kernel owns the shared memory here; the table is not offered to the driver.
 */
#ifndef _LX10K_LINUX_SOC_QCOM_SMEM_H
#define _LX10K_LINUX_SOC_QCOM_SMEM_H

#include <linux/err.h>
#include <linux/errno.h>

#define QCOM_SMEM_HOST_ANY	-1

static inline void *qcom_smem_get(unsigned host, unsigned item, size_t *size) { return (ERR_PTR(-ENOENT)); }

#endif
