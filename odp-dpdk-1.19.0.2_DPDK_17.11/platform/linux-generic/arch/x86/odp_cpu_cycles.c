/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/api/cpu.h>

#include "cpu_flags.h"
#include <odp_init_internal.h>
#include <odp_debug_internal.h>

int _odp_cpu_cycles_init_global(void)
{
	if (cpu_flags_has_rdtsc() == 0) {
		ODP_ERR("RDTSC instruction not supported\n");
		return -1;
	}

	return 0;
}
