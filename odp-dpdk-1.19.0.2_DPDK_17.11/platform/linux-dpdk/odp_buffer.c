/* Copyright (c) 2013-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/api/buffer.h>
#include <odp_buffer_internal.h>
#include <odp_debug_internal.h>
#include <odp_pool_internal.h>
#include <odp/api/plat/buffer_inline_types.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <odp/visibility_begin.h>

/* Fill in buffer header field offsets for inline functions */
const _odp_buffer_inline_offset_t ODP_ALIGNED_CACHE
_odp_buffer_inline_offset = {
	.event_type = offsetof(odp_buffer_hdr_t, event_type)
};

#include <odp/visibility_end.h>

odp_buffer_t odp_buffer_from_event(odp_event_t ev)
{
	return (odp_buffer_t)ev;
}

odp_event_t odp_buffer_to_event(odp_buffer_t buf)
{
	return (odp_event_t)buf;
}

void *odp_buffer_addr(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = buf_hdl_to_hdr(buf);

	return hdr->mb.buf_addr;
}

uint32_t odp_buffer_size(odp_buffer_t buf)
{
	struct rte_mbuf *mbuf = buf_to_mbuf(buf);

	return mbuf->buf_len;
}

int _odp_buffer_type(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = buf_hdl_to_hdr(buf);

	return hdr->type;
}

void _odp_buffer_type_set(odp_buffer_t buf, int type)
{
	odp_buffer_hdr_t *hdr = buf_hdl_to_hdr(buf);

	hdr->type = type;
}

int odp_buffer_is_valid(odp_buffer_t buf)
{
	/* We could call rte_mbuf_sanity_check, but that panics
	 * and aborts the program */
	return buf != ODP_BUFFER_INVALID;
}

int odp_buffer_snprint(char *str, uint32_t n, odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr;
	pool_t *pool;
	int len = 0;

	if (!odp_buffer_is_valid(buf)) {
		ODP_PRINT("Buffer is not valid.\n");
		return len;
	}

	hdr = buf_hdl_to_hdr(buf);
	pool = hdr->pool_ptr;

	len += snprintf(&str[len], n - len,
			"Buffer\n");
	len += snprintf(&str[len], n - len,
			"  pool         %" PRIu64 "\n",
			odp_pool_to_u64(pool->pool_hdl));
	len += snprintf(&str[len], n - len,
			"  phy_addr     %"PRIu64"\n", hdr->mb.buf_physaddr);
	len += snprintf(&str[len], n - len,
			"  addr         %p\n",        hdr->mb.buf_addr);
	len += snprintf(&str[len], n - len,
			"  size         %u\n",        hdr->mb.buf_len);
	len += snprintf(&str[len], n - len,
			"  ref_count    %i\n",
			rte_mbuf_refcnt_read(&hdr->mb));
	len += snprintf(&str[len], n - len,
			"  odp type     %i\n",        hdr->type);

	return len;
}

void odp_buffer_print(odp_buffer_t buf)
{
	int max_len = 512;
	char str[max_len];
	int len;

	len = odp_buffer_snprint(str, max_len - 1, buf);
	str[len] = 0;

	ODP_PRINT("\n%s\n", str);
}

uint64_t odp_buffer_to_u64(odp_buffer_t hdl)
{
	return _odp_pri(hdl);
}
