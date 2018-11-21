/* Copyright (c) 2015, ENEA Software AB
 * Copyright (c) 2015, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include "ofpi.h"
#include "ofpi_pkt_processing.h"
#include "ofpi_util.h"
#include "ofpi_log.h"
#include "ofpi_debug.h"
#include "ofpi_stat.h"


static __thread struct burst_send {
	odp_packet_t *pkt_tbl;
	uint32_t pkt_tbl_cnt;
} send_pkt_tbl[NUM_PORTS] __attribute__((__aligned__(ODP_CACHE_LINE_SIZE)));

static __thread uint32_t tx_burst;

static inline void
send_table(struct ofp_ifnet *ifnet, odp_packet_t *pkt_tbl,
		uint32_t *pkt_tbl_cnt)
{
	int pkts_sent;

	pkts_sent = ofp_send_pkt_multi(ifnet, pkt_tbl, *pkt_tbl_cnt,
			odp_cpu_id());

	if (pkts_sent < 0)
		pkts_sent = 0;
	else
		OFP_UPDATE_PACKET_STAT(tx_fp, pkts_sent);

	if (pkts_sent < (int)(*pkt_tbl_cnt)) {
		int pkt_cnt = (int)(*pkt_tbl_cnt);

		OFP_DBG("odp_pktio_send failed: %d/%d packets dropped",
			pkt_cnt - pkts_sent, pkt_cnt);

		for (; pkts_sent < pkt_cnt; pkts_sent++)
			odp_packet_free(pkt_tbl[pkts_sent]);
	}

	*pkt_tbl_cnt = 0;
}

enum ofp_return_code send_pkt_out(struct ofp_ifnet *dev,
	odp_packet_t pkt)
{
	uint32_t *pkt_tbl_cnt = &send_pkt_tbl[dev->port].pkt_tbl_cnt;
	odp_packet_t *pkt_tbl = (odp_packet_t *)send_pkt_tbl[dev->port].pkt_tbl;

	pkt_tbl[(*pkt_tbl_cnt)++] = pkt;

	OFP_DEBUG_PACKET(OFP_DEBUG_PKT_SEND_NIC, pkt, dev->port);

	if ((*pkt_tbl_cnt) == tx_burst)
		send_table(ofp_get_ifnet(dev->port, 0),
			   pkt_tbl,
			   pkt_tbl_cnt);

	return OFP_PKT_PROCESSED;
}

static void ofp_send_pending_pkt_nocheck(void)
{
	uint32_t i;
	uint32_t *pkt_tbl_cnt;
	odp_packet_t *pkt_tbl;

	for (i = 0; i < NUM_PORTS; i++) {
		pkt_tbl_cnt = &send_pkt_tbl[i].pkt_tbl_cnt;

		if  (!(*pkt_tbl_cnt))
			continue;

		pkt_tbl = (odp_packet_t *)send_pkt_tbl[i].pkt_tbl;

		send_table(ofp_get_ifnet(i, 0), pkt_tbl, pkt_tbl_cnt);
	}
}

enum ofp_return_code ofp_send_pending_pkt(void)
{
	if (tx_burst > 1)
		ofp_send_pending_pkt_nocheck();
	return OFP_PKT_PROCESSED;
}

static __thread void *pkt_tbl = NULL;

int ofp_send_pkt_out_init_local(void)
{
	uint32_t i, j;

	tx_burst = global_param->pkt_tx_burst_size;

	pkt_tbl = malloc(global_param->pkt_tx_burst_size
			 * sizeof(odp_packet_t) * NUM_PORTS + ODP_CACHE_LINE_SIZE);
	if (!pkt_tbl) {
		OFP_ERR("Packet table allocation failed\n");
		ofp_send_pkt_out_term_local();
		return -1;
	}

	for (i = 0; i < NUM_PORTS; i++) {
		send_pkt_tbl[i].pkt_tbl_cnt = 0;
		if (!i) {
			const uint64_t mask = ODP_CACHE_LINE_SIZE - 1;
			send_pkt_tbl[i].pkt_tbl =
				(odp_packet_t *)(((uint64_t)pkt_tbl + mask) & ~mask);
		} else {
			send_pkt_tbl[i].pkt_tbl =
				send_pkt_tbl[0].pkt_tbl + global_param->pkt_tx_burst_size * i;
		}
		for (j = 0; j < global_param->pkt_tx_burst_size; j++)
			send_pkt_tbl[i].pkt_tbl[j] = ODP_PACKET_INVALID;
	}

	return 0;
}

int ofp_send_pkt_out_term_local(void)
{
	uint32_t i, j;

	for (i = 0; i < NUM_PORTS; i++) {

		for (j = 0; j < send_pkt_tbl[i].pkt_tbl_cnt; j++)
			odp_packet_free(send_pkt_tbl[i].pkt_tbl[j]);

		send_pkt_tbl[i].pkt_tbl_cnt = 0;
	}

	free(pkt_tbl);

	return 0;
}
