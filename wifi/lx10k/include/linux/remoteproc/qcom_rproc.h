/*
 * The modem's restart notifications, which snoc.c listens to ("mpss"). The kernel starts the modem
 * before the driver and stops it after, so none ever comes while the driver runs.
 */
#ifndef _LX10K_LINUX_REMOTEPROC_QCOM_RPROC_H
#define _LX10K_LINUX_REMOTEPROC_QCOM_RPROC_H

#include <linux/notifier.h>

enum qcom_ssr_notify_type {
	QCOM_SSR_BEFORE_POWERUP,
	QCOM_SSR_AFTER_POWERUP,
	QCOM_SSR_BEFORE_SHUTDOWN,
	QCOM_SSR_AFTER_SHUTDOWN,
};

struct qcom_ssr_notify_data {
	const char *name;
	bool crashed;
};

void *qcom_register_ssr_notifier(const char *name, struct notifier_block *nb);
int qcom_unregister_ssr_notifier(void *notify, struct notifier_block *nb);

#endif
