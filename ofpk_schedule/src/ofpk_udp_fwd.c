/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
 


/*----------------------------------------------*
* 包含头文件                              	        *
*----------------------------------------------*/
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ofpk.h"
#include "ofpk_paras.h"

/*----------------------------------------------*
* 外部变量说明                           		        *
*----------------------------------------------*/

/*----------------------------------------------*
* 内部函数原型说明                     			        *
*----------------------------------------------*/

/*----------------------------------------------*
* 外部函数原型说明                             			*
*----------------------------------------------*/

static int epfd;

struct ofp_sockaddr_in *raddr = NULL;


static int create_local_sock(int lport, char *laddr_txt, int bind_any)
{
	struct ofp_sockaddr_in laddr = {0};
	int sd;
	
#ifdef ENABLE_MULTIPLE_IP
	int optval;
#endif

	/* Create socket*/
	if ((sd = ofp_socket(OFP_AF_INET, OFP_SOCK_DGRAM, OFP_IPPROTO_UDP)) < 0) {
		OFPK_ERR("Error: Failed to create socket: errno = %s!\n", ofp_strerror(ofp_errno));
		return OFPK_ERROR;
	}

	memset(&laddr, 0, sizeof(laddr));
	laddr.sin_family = OFP_AF_INET;
	laddr.sin_port   = odp_cpu_to_be_16(lport);
	laddr.sin_addr.s_addr = inet_addr(laddr_txt);
	laddr.sin_len = sizeof(laddr);

#ifdef ENABLE_MULTIPLE_IP
	if (bind_any) {
		optval = 1;
		ofp_setsockopt(sd, OFP_SOL_SOCKET, OFP_SO_BINDANY, &optval, sizeof(&optval));
	}
#endif
	/* Bind to local address*/
	if (ofp_bind(sd, (struct ofp_sockaddr *)&laddr, sizeof(struct ofp_sockaddr)) < 0) {
		OFPK_ERR("Error: Failed to bind: addr=%s, port=%d: errno=%s\n",
					laddr_txt, lport, ofp_strerror(ofp_errno));
		ofp_close(sd);
		return -1;
	}
			   
    OFP_INFO("115832...new fd(%d),laddr=%s,lport=%d", sd, laddr_txt, lport);

			   
	struct ofp_epoll_event e = { OFP_EPOLLIN, { .fd = sd } };
	ofp_epoll_ctl(epfd, OFP_EPOLL_CTL_ADD, sd , &e);
	return 0;
}



static void recv_sendto(int sd)
{
	char buf[1500];
	int len = sizeof(buf);
	
	len = ofp_recv(sd, buf, len, 0);
	if (len == -1) {
		OFPK_ERR("Faild to rcv data(errno = %d)\n", ofp_errno);
		return;
	}

	ofp_sendto(sd, buf, len, 0, (struct ofp_sockaddr *)raddr, sizeof(*raddr));

}



static void reading(void)
{
	int r, i;
	struct ofp_epoll_event events[32];

	r = ofp_epoll_wait(epfd, events, 32, 0);

	for (i = 0; i < r; ++i) {
		recv_sendto(events[i].data.fd);
	}

}


void *udp_fwd_start(void *sock_arg)
{
	int port_idx, vlan = 0, port;
	uint32_t destaddr;
	char dev[16];
	int ret;

	gbl_args_t *gbl_args = (gbl_args_t *)sock_arg;

	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed.\n");
		return NULL;
	}

	epfd = ofp_epoll_create(1);

	for (port_idx = 0; port_idx < gbl_args->appl_args.sock_count; port_idx++) {
		ret = create_local_sock(TEST_CPORT + port_idx, gbl_args->appl_args.laddr, 0);
		if (ret == -1)
			return NULL;
	}

#ifdef ENABLE_MULTIPLE_IP
	strcpy(dev, "fp0\0");
	gbl_args->appl_args.laddr[strlen(gbl_args->appl_args.laddr)-1]++;
	destaddr = inet_addr(gbl_args->appl_args.laddr);

	port = ofp_name_to_port_vlan(dev, &vlan);
	ofp_set_route_params(OFP_ROUTE_ADD, 0 /*vrf*/, vlan, port, 0,  destaddr, 32, 0, OFP_RTF_LOCAL);

	ret = create_local_sock(TEST_CPORT + 1, gbl_args->appl_args.laddr, 1);
	if (ret == -1)
		return NULL;
#endif

	/* Allocate remote address - will be used in notification function*/
	raddr = malloc(sizeof(struct ofp_sockaddr_in));
	if (raddr == NULL) {
		OFPK_ERR("Error: Failed allocate memory: errno = %s\n", ofp_strerror(ofp_errno));
		return NULL;
	}
	memset(raddr, 0, sizeof(*raddr));
	raddr->sin_family = OFP_AF_INET;
	raddr->sin_port   = odp_cpu_to_be_16(TEST_SPORT);
	raddr->sin_addr.s_addr = inet_addr(gbl_args->appl_args.laddr);
	raddr->sin_len         = sizeof(*raddr);

	while (1) {
		reading();
	}

	return NULL;
}


