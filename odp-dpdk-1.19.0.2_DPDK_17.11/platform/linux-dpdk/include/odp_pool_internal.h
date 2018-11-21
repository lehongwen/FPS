/* Copyright (c) 2013-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP buffer pool - internal header
 */

#ifndef ODP_POOL_INTERNAL_H_
#define ODP_POOL_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/std_types.h>
#include <odp/api/pool.h>
#include <odp_buffer_internal.h>
#include <odp/api/packet_io.h>
#include <odp/api/align.h>
#include <odp/api/hints.h>
#include <odp_config_internal.h>
#include <odp/api/debug.h>
#include <odp_debug_internal.h>
#include <odp/api/plat/strong_types.h>
#include <string.h>

/* for DPDK */
#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

/* Use ticketlock instead of spinlock */
#define POOL_USE_TICKETLOCK

/* Extra error checks */
/* #define POOL_ERROR_CHECK */

#ifdef POOL_USE_TICKETLOCK
#include <odp/api/ticketlock.h>
#else
#include <odp/api/spinlock.h>
#endif

typedef struct ODP_ALIGNED_CACHE {
#ifdef POOL_USE_TICKETLOCK
	odp_ticketlock_t ODP_ALIGNED_CACHE lock;
#else
	odp_spinlock_t ODP_ALIGNED_CACHE lock;
#endif
	char			name[ODP_POOL_NAME_LEN];
	odp_pool_param_t	params;
	odp_pool_t		pool_hdl;
	struct rte_mempool	*rte_mempool;

} pool_t;

typedef struct pool_table_t {
	pool_t		pool[ODP_CONFIG_POOLS];
	odp_shm_t	shm;
} pool_table_t;

extern pool_table_t *pool_tbl;

static inline pool_t *pool_entry(uint32_t pool_idx)
{
	return &pool_tbl->pool[pool_idx];
}

static inline pool_t *pool_entry_from_hdl(odp_pool_t pool_hdl)
{
	return &pool_tbl->pool[_odp_typeval(pool_hdl)];
}

static inline void buffer_free_multi(odp_buffer_hdr_t *buf_hdr[], int num)
{
	int i;

	for (i = 0; i < num; i++)
		rte_ctrlmbuf_free((struct rte_mbuf *)(uintptr_t)buf_hdr[i]);
}

#ifdef __cplusplus
}
#endif

#endif /* ODP_POOL_INTERNAL_H_ */
