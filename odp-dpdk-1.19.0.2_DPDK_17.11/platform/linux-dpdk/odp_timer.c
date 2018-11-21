/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include "config.h"

#include <string.h>

#include <odp/api/shared_memory.h>
#include <odp/api/ticketlock.h>
#include <odp/api/timer.h>

#include <odp_init_internal.h>
#include <odp_debug_internal.h>
#include <odp_ring_internal.h>
#include <odp_timer_internal.h>

#include <rte_cycles.h>
#include <rte_timer.h>

/* TODO: timer ABI spec needs update
 * - ODP_TIMER_INVALID: 0xffffffff -> NULL
 * - Remove "struct timer_pool_s"
 */

/* TODO: Add this to queue interface. Sets a queue to poll timer on dequeue */
void queue_enable_timer_poll(odp_queue_t queue);

/* Timer states */
#define NOT_TICKING 0
#define EXPIRED     1
#define TICKING     2

/* One second in nanoseconds */
#define SEC_IN_NS ((uint64_t)1000000000)

/* Maximum number of timer pools */
#define MAX_TIMER_POOLS  8

/* Maximum number of timers per timer pool. Must be a power of two.
 * Validation test expects 2000 timers per thread and up to 32 threads. */
#define MAX_TIMERS       (32 * 1024)

/* Actual resolution depends on application polling frequency. Promise
 * 10 usec resolution. */
#define MAX_RES_NS       10000

/* Limit minimum supported timeout in timer (CPU) cycles. Timer setup, polling,
 * timer management, timeout enqueue, etc takes about this many CPU cycles.
 * It does not make sense to set up shorter timeouts than this. */
#define MIN_TMO_CYCLES   2000

/* Duration of a spin loop */
#define WAIT_SPINS 30

typedef struct {
	odp_ticketlock_t     lock;
	int                  state;
	uint64_t             tick;
	void                *user_ptr;
	odp_queue_t          queue;
	odp_event_t          tmo_event;
	struct timer_pool_s *timer_pool;
	uint32_t             timer_idx;

	struct rte_timer     rte_timer;

} timer_entry_t;

typedef struct timer_pool_s {
	timer_entry_t timer[MAX_TIMERS];

	struct {
		uint32_t ring_mask;

		ring_t   ring_hdr;
		uint32_t ring_data[MAX_TIMERS];

	} free_timer;

	odp_timer_pool_param_t param;
	char name[ODP_TIMER_POOL_NAME_LEN + 1];
	int used;
	odp_ticketlock_t lock;
	uint32_t cur_timers;
	uint32_t hwm_timers;

} timer_pool_t;

typedef struct {
	timer_pool_t timer_pool[MAX_TIMER_POOLS];
	odp_shm_t shm;
	odp_ticketlock_t lock;
	volatile uint64_t wait_counter;

} timer_global_t;

timer_global_t *timer_global;

/* Enable timer polling from scheduler or queue dequeue. */
odp_bool_t inline_timers = true;

static inline timer_entry_t *timer_from_hdl(odp_timer_t timer_hdl)
{
	return (timer_entry_t *)(uintptr_t)timer_hdl;
}

int odp_timer_init_global(const odp_init_t *params)
{
	odp_shm_t shm;

	if (params)
		if (params->not_used.feat.timer) {
			/* Timer is not initialized. Disable _timer_run()
			 * calls. */
			inline_timers = false;
			return 0;
	}

	shm = odp_shm_reserve("timer_global", sizeof(timer_global_t),
			      ODP_CACHE_LINE_SIZE, 0);

	if (shm == ODP_SHM_INVALID) {
		ODP_ERR("Global data alloc (%zu bytes) failed\n",
			sizeof(timer_global_t));
		return -1;
	}

	timer_global = odp_shm_addr(shm);
	memset(timer_global, 0, sizeof(timer_global_t));

	timer_global->shm = shm;
	odp_ticketlock_init(&timer_global->lock);

	rte_timer_subsystem_init();

	return 0;
}

int odp_timer_term_global(void)
{
	if (timer_global == NULL)
		return 0;

	odp_shm_free(timer_global->shm);
	return 0;
}

unsigned int _timer_run(void)
{
	/* TODO: limit rte_timer_manage call rate e.g. by recording/comparing
	 * cycle counter values. */
	rte_timer_manage();

	return 0;
}

int odp_timer_capability(odp_timer_clk_src_t clk_src,
			 odp_timer_capability_t *capa)
{
	if (clk_src != ODP_CLOCK_CPU) {
		ODP_ERR("Clock source not supported\n");
		return -1;
	}

	memset(capa, 0, sizeof(odp_timer_capability_t));

	capa->highest_res_ns = MAX_RES_NS;

	return 0;
}

odp_timer_pool_t odp_timer_pool_create(const char *name,
				       const odp_timer_pool_param_t *param)
{
	timer_pool_t *timer_pool;
	timer_entry_t *timer;
	uint32_t i, num_timers;

	if (param->res_ns < MAX_RES_NS) {
		ODP_ERR("Too high resolution\n");
		return ODP_TIMER_POOL_INVALID;
	}

	num_timers = param->num_timers;
	num_timers = ROUNDUP_POWER2_U32(num_timers);

	if (num_timers > MAX_TIMERS) {
		ODP_ERR("Too many timers\n");
		return ODP_TIMER_POOL_INVALID;
	}

	odp_ticketlock_lock(&timer_global->lock);

	for (i = 0; i < MAX_TIMER_POOLS; i++) {
		timer_pool = &timer_global->timer_pool[i];

		if (timer_pool->used == 0) {
			timer_pool->used = 1;
			break;
		}
	}

	odp_ticketlock_unlock(&timer_global->lock);

	if (i == MAX_TIMER_POOLS) {
		ODP_ERR("No free timer pools\n");
		return ODP_TIMER_POOL_INVALID;
	}

	if (name) {
		strncpy(timer_pool->name, name,
			ODP_TIMER_POOL_NAME_LEN);
		timer_pool->name[ODP_TIMER_POOL_NAME_LEN] = 0;
	}

	timer_pool->param = *param;

	ring_init(&timer_pool->free_timer.ring_hdr);
	timer_pool->free_timer.ring_mask = num_timers - 1;

	odp_ticketlock_init(&timer_pool->lock);
	timer_pool->cur_timers = 0;
	timer_pool->hwm_timers = 0;

	for (i = 0; i < num_timers; i++) {
		timer = &timer_pool->timer[i];
		memset(timer, 0, sizeof(timer_entry_t));

		odp_ticketlock_init(&timer->lock);
		rte_timer_init(&timer->rte_timer);
		timer->timer_pool = timer_pool;
		timer->timer_idx  = i;

		ring_enq(&timer_pool->free_timer.ring_hdr,
			 timer_pool->free_timer.ring_mask, i);
	}

	return timer_pool;
}

void odp_timer_pool_start(void)
{
}

void odp_timer_pool_destroy(odp_timer_pool_t tp)
{
	timer_pool_t *timer_pool = tp;

	odp_ticketlock_lock(&timer_global->lock);

	timer_pool->used = 0;

	odp_ticketlock_unlock(&timer_global->lock);
}

uint64_t odp_timer_tick_to_ns(odp_timer_pool_t tp, uint64_t ticks)
{
	uint64_t nsec;
	uint64_t freq_hz = rte_get_timer_hz();
	uint64_t sec = 0;
	(void)tp;

	if (ticks >= freq_hz) {
		sec   = ticks / freq_hz;
		ticks = ticks - sec * freq_hz;
	}

	nsec = (SEC_IN_NS * ticks) / freq_hz;

	return (sec * SEC_IN_NS) + nsec;
}

uint64_t odp_timer_ns_to_tick(odp_timer_pool_t tp, uint64_t ns)
{
	uint64_t ticks;
	uint64_t freq_hz = rte_get_timer_hz();
	uint64_t sec = 0;
	(void)tp;

	if (ns >= SEC_IN_NS) {
		sec = ns / SEC_IN_NS;
		ns  = ns - sec * SEC_IN_NS;
	}

	ticks  = sec * freq_hz;
	ticks += (ns * freq_hz) / SEC_IN_NS;

	return ticks;
}

uint64_t odp_timer_current_tick(odp_timer_pool_t tp)
{
	(void)tp;

	return rte_get_timer_cycles();
}

int odp_timer_pool_info(odp_timer_pool_t tp,
			odp_timer_pool_info_t *info)
{
	timer_pool_t *timer_pool = tp;

	info->param      = timer_pool->param;
	info->cur_timers = timer_pool->cur_timers;
	info->hwm_timers = timer_pool->hwm_timers;
	info->name       = timer_pool->name;
	return 0;
}

uint64_t odp_timer_pool_to_u64(odp_timer_pool_t tp)
{
	return (uint64_t)(uintptr_t)tp;
}

odp_timer_t odp_timer_alloc(odp_timer_pool_t tp,
			    odp_queue_t queue,
			    void *user_ptr)
{
	uint32_t timer_idx;
	timer_entry_t *timer;
	timer_pool_t *timer_pool = tp;

	if (odp_unlikely(tp == ODP_TIMER_POOL_INVALID)) {
		ODP_ERR("Invalid timer pool.\n");
		return ODP_TIMER_INVALID;
	}

	if (odp_unlikely(queue == ODP_QUEUE_INVALID)) {
		ODP_ERR("%s: Invalid queue handle.\n", timer_pool->name);
		return ODP_TIMER_INVALID;
	}

	timer_idx = ring_deq(&timer_pool->free_timer.ring_hdr,
			     timer_pool->free_timer.ring_mask);

	if (timer_idx == RING_EMPTY)
		return ODP_TIMER_INVALID;

	timer = &timer_pool->timer[timer_idx];

	timer->state     = NOT_TICKING;
	timer->user_ptr  = user_ptr;
	timer->queue     = queue;
	timer->tmo_event = ODP_EVENT_INVALID;

	/* Enable timer polling from dequeue operation. Scheduler polls timer
	 * by default. */
	if (odp_queue_type(queue) == ODP_QUEUE_TYPE_PLAIN)
		queue_enable_timer_poll(queue);

	odp_ticketlock_lock(&timer_pool->lock);

	timer_pool->cur_timers++;

	if (timer_pool->cur_timers > timer_pool->hwm_timers)
		timer_pool->hwm_timers = timer_pool->cur_timers;

	odp_ticketlock_unlock(&timer_pool->lock);

	return (odp_timer_t)timer;
}

odp_event_t odp_timer_free(odp_timer_t timer_hdl)
{
	odp_event_t ev;
	timer_entry_t *timer = timer_from_hdl(timer_hdl);
	timer_pool_t *timer_pool = timer->timer_pool;
	uint32_t timer_idx = timer->timer_idx;

retry:
	odp_ticketlock_lock(&timer->lock);

	if (timer->state == TICKING) {
		ODP_DBG("Freeing active timer.\n");

		if (rte_timer_stop(&timer->rte_timer)) {
			/* Another core runs timer callback function. */
			odp_ticketlock_unlock(&timer->lock);
			goto retry;
		}

		ev = timer->tmo_event;
		timer->tmo_event = ODP_EVENT_INVALID;
		timer->state = NOT_TICKING;
	} else {
		ev = ODP_EVENT_INVALID;
	}

	odp_ticketlock_unlock(&timer->lock);

	/* TODO: disable timer polling from queue. Needs better queue <-> timer
	 * interface. */

	odp_ticketlock_lock(&timer_pool->lock);

	timer_pool->cur_timers--;

	odp_ticketlock_unlock(&timer_pool->lock);

	ring_enq(&timer_pool->free_timer.ring_hdr,
		 timer_pool->free_timer.ring_mask, timer_idx);

	return ev;
}

static inline odp_timeout_hdr_t *timeout_to_hdr(odp_timeout_t tmo)
{
	return (odp_timeout_hdr_t *)(uintptr_t)tmo;
}

static void timer_cb(struct rte_timer *rte_timer, void *arg)
{
	timer_entry_t *timer = arg;
	odp_event_t event;
	odp_queue_t queue;
	(void)rte_timer;

	odp_ticketlock_lock(&timer->lock);

	if (timer->state != TICKING) {
		ODP_ERR("Timer has been cancelled or freed.\n");
		odp_ticketlock_unlock(&timer->lock);
		return;
	}

	queue = timer->queue;
	event = timer->tmo_event;
	timer->tmo_event = ODP_EVENT_INVALID;
	timer->state = EXPIRED;

	odp_ticketlock_unlock(&timer->lock);

	if (odp_unlikely(odp_queue_enq(queue, event))) {
		ODP_ERR("Timeout event enqueue failed.\n");
		odp_event_free(event);
	}
}

static inline int timer_set(odp_timer_t timer_hdl, uint64_t tick,
			    odp_event_t *event, int absolute)
{
	odp_event_t old_ev, tmo_event;
	uint64_t cur_tick, rel_tick, abs_tick;
	timer_entry_t *timer = timer_from_hdl(timer_hdl);
	int num_retry = 0;
	unsigned int lcore = rte_lcore_id();

retry:
	cur_tick = rte_get_timer_cycles();

	if (absolute) {
		abs_tick = tick;
		rel_tick = abs_tick - cur_tick;

		if (odp_unlikely(abs_tick < cur_tick))
			rel_tick = 0;
	} else {
		rel_tick = tick;
		abs_tick = rel_tick + cur_tick;
	}

	if (rel_tick < MIN_TMO_CYCLES) {
		ODP_DBG("Too early\n");
		ODP_DBG("  cur_tick %" PRIu64 ", abs_tick %" PRIu64 "\n",
			cur_tick, abs_tick);
		ODP_DBG("  num_retry %i\n", num_retry);
		return ODP_TIMER_TOOEARLY;
	}

	odp_ticketlock_lock(&timer->lock);

	if (timer->tmo_event == ODP_EVENT_INVALID)
		if (event == NULL || (event && *event == ODP_EVENT_INVALID)) {
			odp_ticketlock_unlock(&timer->lock);
			/* Event missing, or timer already expired and
			 * enqueued the event. */
			return ODP_TIMER_NOEVENT;
	}

	if (odp_unlikely(rte_timer_reset(&timer->rte_timer, rel_tick, SINGLE,
					 lcore, timer_cb, timer))) {
		int do_retry = 0;

		/* Another core is currently running the callback function.
		 * State is:
		 * - TICKING, when callback has not yet started
		 * - EXPIRED, when callback has not yet finished, or this cpu
		 *            does not yet see that it has been finished
		 */

		if (timer->state == EXPIRED)
			do_retry = 1;

		odp_ticketlock_unlock(&timer->lock);

		if (do_retry) {
			/* Timer has been expired, wait and retry until DPDK on
			 * this CPU sees it. */
			int i;

			for (i = 0; i < WAIT_SPINS; i++)
				timer_global->wait_counter++;

			num_retry++;
			goto retry;
		}

		/* Timer was just about to expire. Too late to reset this timer.
		 * Return code is NOEVENT, even when application did give
		 * an event. */
		return ODP_TIMER_NOEVENT;
	}

	if (event) {
		old_ev = timer->tmo_event;

		if (*event != ODP_EVENT_INVALID)
			timer->tmo_event = *event;

		*event = old_ev;
	}

	tmo_event    = timer->tmo_event;
	timer->tick  = abs_tick;
	timer->state = TICKING;

	if (odp_event_type(tmo_event) == ODP_EVENT_TIMEOUT) {
		odp_timeout_hdr_t *timeout_hdr;

		timeout_hdr = timeout_to_hdr((odp_timeout_t)tmo_event);
		timeout_hdr->expiration = abs_tick;
		timeout_hdr->user_ptr   = timer->user_ptr;
		timeout_hdr->timer      = (odp_timer_t)timer;
	}

	odp_ticketlock_unlock(&timer->lock);
	return ODP_TIMER_SUCCESS;
}

int odp_timer_set_abs(odp_timer_t timer_hdl, uint64_t abs_tick,
		      odp_event_t *tmo_ev)
{
	return timer_set(timer_hdl, abs_tick, tmo_ev, 1);
}

int odp_timer_set_rel(odp_timer_t timer_hdl, uint64_t rel_tick,
		      odp_event_t *tmo_ev)
{
	return timer_set(timer_hdl, rel_tick, tmo_ev, 0);
}

int odp_timer_cancel(odp_timer_t timer_hdl, odp_event_t *tmo_ev)
{
	timer_entry_t *timer = timer_from_hdl(timer_hdl);

	odp_ticketlock_lock(&timer->lock);

	if (odp_unlikely(timer->state < TICKING)) {
		odp_ticketlock_unlock(&timer->lock);
		return -1;
	}

	if (odp_unlikely(rte_timer_stop(&timer->rte_timer))) {
		/* Another core runs timer callback function. */
		odp_ticketlock_unlock(&timer->lock);
		return -1;
	}

	*tmo_ev = timer->tmo_event;
	timer->tmo_event = ODP_EVENT_INVALID;
	timer->state = NOT_TICKING;

	odp_ticketlock_unlock(&timer->lock);
	return 0;
}

uint64_t odp_timer_to_u64(odp_timer_t timer_hdl)
{
	return (uint64_t)(uintptr_t)timer_hdl;
}

odp_timeout_t odp_timeout_from_event(odp_event_t ev)
{
	return (odp_timeout_t)ev;
}

odp_event_t odp_timeout_to_event(odp_timeout_t tmo)
{
	return (odp_event_t)tmo;
}

uint64_t odp_timeout_to_u64(odp_timeout_t tmo)
{
	return (uint64_t)(uintptr_t)tmo;
}

int odp_timeout_fresh(odp_timeout_t tmo)
{
	odp_timeout_hdr_t *timeout_hdr = timeout_to_hdr(tmo);
	timer_entry_t *timer = timer_from_hdl(timeout_hdr->timer);

	/* Check if timer has been reused after timeout sent. */
	return timeout_hdr->expiration == timer->tick;
}

odp_timer_t odp_timeout_timer(odp_timeout_t tmo)
{
	odp_timeout_hdr_t *timeout_hdr = timeout_to_hdr(tmo);

	return timeout_hdr->timer;
}

uint64_t odp_timeout_tick(odp_timeout_t tmo)
{
	odp_timeout_hdr_t *timeout_hdr = timeout_to_hdr(tmo);

	return timeout_hdr->expiration;
}

void *odp_timeout_user_ptr(odp_timeout_t tmo)
{
	odp_timeout_hdr_t *timeout_hdr = timeout_to_hdr(tmo);

	return timeout_hdr->user_ptr;
}

odp_timeout_t odp_timeout_alloc(odp_pool_t pool)
{
	odp_buffer_t buf = odp_buffer_alloc(pool);

	if (odp_unlikely(buf == ODP_BUFFER_INVALID))
		return ODP_TIMEOUT_INVALID;
	return odp_timeout_from_event(odp_buffer_to_event(buf));
}

void odp_timeout_free(odp_timeout_t tmo)
{
	odp_event_t ev = odp_timeout_to_event(tmo);

	odp_buffer_free(odp_buffer_from_event(ev));
}
