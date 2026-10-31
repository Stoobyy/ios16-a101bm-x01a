/*
 * Supplies as ath10k's snoc.c asks for them: five, by name (vdd-0.8-cx-mx, vdd-1.8-xo, vdd-1.3-rfa,
 * vdd-3.3-ch0, vdd-3.3-ch1). On the X01A the kernel switches the same five on through RPMh before the
 * driver starts (bsd/dev/hakios.wcn3990.c, power_on()), so here they are there and on.
 */
#ifndef _LX10K_LINUX_REGULATOR_CONSUMER_H
#define _LX10K_LINUX_REGULATOR_CONSUMER_H

#include <linux/err.h>

struct device;
struct regulator;

struct regulator_bulk_data {
	const char *supply;
	struct regulator *consumer;
	int init_load_uA;
	int ret;
};

int devm_regulator_bulk_get(struct device *dev, int count, struct regulator_bulk_data *supplies);
int regulator_bulk_get(struct device *dev, int count, struct regulator_bulk_data *supplies);
int regulator_bulk_enable(int count, struct regulator_bulk_data *supplies);
int regulator_bulk_disable(int count, struct regulator_bulk_data *supplies);
static inline int regulator_set_load(struct regulator *regulator, int load) { return (0); }
static inline int regulator_set_voltage(struct regulator *regulator, int min, int max) { return (0); }

#endif
