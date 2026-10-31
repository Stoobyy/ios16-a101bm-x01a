/*
 * Power sequencing: the X01A has no WCN399x PMU node, so the driver is told there is none to be had
 * (-EPROBE_DEFER, which snoc.c takes for "use the supplies directly").
 */
#ifndef _LX10K_LINUX_PWRSEQ_CONSUMER_H
#define _LX10K_LINUX_PWRSEQ_CONSUMER_H

#include <linux/err.h>
#include <linux/errno.h>

struct device;
struct pwrseq_desc;

static inline struct pwrseq_desc *devm_pwrseq_get(struct device *dev, const char *target) { return (ERR_PTR(-EPROBE_DEFER)); }
static inline int pwrseq_power_on(struct pwrseq_desc *desc) { return (0); }
static inline int pwrseq_power_off(struct pwrseq_desc *desc) { return (0); }

#endif
