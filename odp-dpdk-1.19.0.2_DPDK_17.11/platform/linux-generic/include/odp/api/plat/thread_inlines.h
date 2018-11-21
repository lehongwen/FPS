/* Copyright (c) 2018-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PLAT_THREAD_INLINES_H_
#define ODP_PLAT_THREAD_INLINES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @cond _ODP_HIDE_FROM_DOXYGEN_ */

typedef struct {
	int thr;
	int cpu;
	odp_thread_type_t type;

} _odp_thread_state_t;

extern __thread _odp_thread_state_t *_odp_this_thread;

#ifndef _ODP_NO_INLINE
	/* Inline functions by default */
	#define _ODP_INLINE static inline
	#define odp_thread_id __odp_thread_id
	#define odp_thread_type __odp_thread_type
	#define odp_cpu_id __odp_cpu_id
#else
	#define _ODP_INLINE
#endif

_ODP_INLINE int odp_thread_id(void)
{
	return _odp_this_thread->thr;
}

_ODP_INLINE odp_thread_type_t odp_thread_type(void)
{
	return _odp_this_thread->type;
}

_ODP_INLINE int odp_cpu_id(void)
{
	return _odp_this_thread->cpu;
}

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif
