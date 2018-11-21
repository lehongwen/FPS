/* Copyright (c) 2017-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP shared memory - implementation internal
 */

#ifndef ODP_SHM_INTERNAL_H_
#define ODP_SHM_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/init.h>

int _odp_shm_init_global(const odp_init_t *init);

int _odp_shm_init_local(void);

int _odp_shm_term_global(void);

int _odp_shm_term_local(void);

#ifdef __cplusplus
}
#endif

#endif
