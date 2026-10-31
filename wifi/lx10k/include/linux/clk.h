/*
 * Clocks as ath10k's snoc.c asks for them: two, both optional ("cxo_ref_clk_pin", "qdss"). The X01A's
 * radio node names none: its reference clock is the boot loader's and the modem's, and stays on.
 */
#ifndef _LX10K_LINUX_CLK_H
#define _LX10K_LINUX_CLK_H

struct device;
struct clk;

struct clk_bulk_data {
	const char *id;
	struct clk *clk;
};

static inline int devm_clk_bulk_get_optional(struct device *dev, int count, struct clk_bulk_data *clks) { return (0); }
static inline int clk_bulk_prepare_enable(int count, const struct clk_bulk_data *clks) { return (0); }
static inline void clk_bulk_disable_unprepare(int count, const struct clk_bulk_data *clks) { }

#endif
