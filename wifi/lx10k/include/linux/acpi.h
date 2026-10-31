/*
 * ACPI as ath10k's mac.c uses it, for one thing: a laptop's WRDD method, its regulatory hint. A phone
 * has no ACPI, so there is no device handle and the driver goes on without the hint. (LinuxKPI's
 * acpi.h brings ACPICA's own types, spelt as ACPICA spells them, not as mac.c does.)
 */
#ifndef _LX10K_LINUX_ACPI_H
#define _LX10K_LINUX_ACPI_H

#include <linux/types.h>

typedef void *acpi_handle;
typedef u32 acpi_status;
typedef char *acpi_string;

#define ACPI_TYPE_INTEGER	0x01
#define ACPI_TYPE_PACKAGE	0x04
#define ACPI_ALLOCATE_BUFFER	((u64)-1)
#define AE_NOT_FOUND		0x0005
#define ACPI_FAILURE(status)	((status) != 0)
#define ACPI_HANDLE(dev)	((acpi_handle)NULL)

union acpi_object {
	u32 type;
	struct {
		u32 type;
		u64 value;
	} integer;
	struct {
		u32 type;
		u32 count;
		union acpi_object *elements;
	} package;
};

struct acpi_buffer {
	u64 length;
	void *pointer;
};

static inline acpi_status
acpi_get_handle(acpi_handle parent, acpi_string path, acpi_handle *handle)
{
	return (AE_NOT_FOUND);
}

static inline acpi_status
acpi_evaluate_object(acpi_handle handle, acpi_string path, void *arguments, struct acpi_buffer *result)
{
	return (AE_NOT_FOUND);
}

#endif
