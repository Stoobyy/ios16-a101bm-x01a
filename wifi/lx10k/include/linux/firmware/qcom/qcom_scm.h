/*
 * TrustZone's memory assignment, which qmi.c asks for the firmware's MSA unless the node says
 * "qcom,msa-fixed-perm". The X01A's node says it (lx10k_platform.c): its TrustZone refuses every such
 * move (-2, 0x56), and Android's driver does not make one either.
 */
#ifndef _LX10K_LINUX_FIRMWARE_QCOM_QCOM_SCM_H
#define _LX10K_LINUX_FIRMWARE_QCOM_QCOM_SCM_H

#include <linux/types.h>

#define QCOM_SCM_VMID_HLOS	0x3
#define QCOM_SCM_VMID_MSS_MSA	0xf
#define QCOM_SCM_VMID_WLAN	0x18
#define QCOM_SCM_VMID_WLAN_CE	0x19
#define QCOM_SCM_PERM_READ	0x4
#define QCOM_SCM_PERM_WRITE	0x2
#define QCOM_SCM_PERM_EXEC	0x1
#define QCOM_SCM_PERM_RW	(QCOM_SCM_PERM_READ | QCOM_SCM_PERM_WRITE)

struct qcom_scm_vmperm {
	int vmid;
	int perm;
};

int qcom_scm_assign_mem(phys_addr_t address, size_t size, u64 *source, const struct qcom_scm_vmperm *next, unsigned int count);

#endif
