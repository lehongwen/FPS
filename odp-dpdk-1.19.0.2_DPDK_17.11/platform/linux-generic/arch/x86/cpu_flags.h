/* Copyright (c) 2017-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PLAT_CPU_FLAGS_H_
#define ODP_PLAT_CPU_FLAGS_H_

#ifdef __cplusplus
extern "C" {
#endif

void cpu_flags_print_all(void);
int cpu_flags_has_rdtsc(void);

#ifdef __cplusplus
}
#endif

#endif
