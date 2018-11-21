/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PACKET_DPDK_H_
#define ODP_PACKET_DPDK_H_

#include <odp/api/packet_io.h>
#include <odp/api/plat/packet_inlines.h>
#include <odp_packet_internal.h>

#include <stdint.h>

/** Packet parser using DPDK interface */
int dpdk_packet_parse_common(packet_parser_t *prs,
			     const uint8_t *ptr,
			     uint32_t pkt_len,
			     uint32_t seg_len,
			     struct rte_mbuf *mbuf,
			     int layer,
			     odp_pktin_config_opt_t pktin_cfg);

static inline int dpdk_packet_parse_layer(odp_packet_hdr_t *pkt_hdr,
					  struct rte_mbuf *mbuf,
					  odp_pktio_parser_layer_t layer,
					  odp_pktin_config_opt_t pktin_cfg)
{
	odp_packet_t pkt = packet_handle(pkt_hdr);
	uint32_t seg_len = odp_packet_seg_len(pkt);
	uint32_t len = odp_packet_len(pkt);
	uint8_t *base = odp_packet_data(pkt);

	return dpdk_packet_parse_common(&pkt_hdr->p, base, len,
					seg_len, mbuf, layer, pktin_cfg);
}

#endif
