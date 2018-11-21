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
#include "ofpk_socket.h"
#include "ofpk_common.h"

/*----------------------------------------------*
* 外部变量说明                           		        *
*----------------------------------------------*/

/** Server socket backlog */
#define TCP_SOCKET_BACKLOG 10

/*----------------------------------------------*
* 内部函数原型说明                     			        *
*----------------------------------------------*/

static int init_local_flags = 0;
static int udp_epfd         = 0;

/*----------------------------------------------*
* 外部函数原型说明                             			*
*----------------------------------------------*/

static  ofpk_server_t    ofpk_server;   //本地udp服务端数据结构  
static  ofpk_client_t    ofpk_client;   //本地udp客户端数据结构

static  int ofpk_send_calls;	 /**< Number of send() function calls */
static  int ofpk_send_bytes;	 /**< Bytes sent */
static  int ofpk_recv_bytes;
static  int ofpk_recv_counts;
 

 void ofp_udp_localInit(void)
{
	if(!init_local_flags)
	{
		memset(&ofpk_server, 0, sizeof( ofpk_server_t ));
		memset(&ofpk_client, 0, sizeof( ofpk_client_t ));
		
		udp_epfd = ofp_epoll_create(1);
		
		init_local_flags = 1;
	}	
}

/*****************************************************************************
  Function:        ofpk_setup_udp_server
  Description:      
  Input:             
  Output:          NA
  Return:           
  Others:         
******************************************************************************/
int ofpk_setup_udp_server(char *laddr, uint16_t lport)
{
	int server_fd     = 0;
	
	struct ofp_sockaddr_in own_addr = {0};

	server_fd = ofp_socket(OFP_AF_INET, OFP_SOCK_DGRAM, OFP_IPPROTO_UDP);
	if (server_fd < 0) {
		OFPK_ERR("Error: ofp_socket failed\n");
		return OFPK_ERROR;
	}
	
	memset(&own_addr, 0, sizeof(own_addr));
	own_addr.sin_len    = sizeof(struct ofp_sockaddr_in);
	own_addr.sin_family = OFP_AF_INET;
	own_addr.sin_port   = odp_cpu_to_be_16(lport);

	if (laddr == NULL) {
		own_addr.sin_addr.s_addr = OFP_INADDR_ANY;
	} else {
		struct in_addr laddr_lin;
		if (inet_aton(laddr, &laddr_lin) == 0) {
			OFPK_ERR("Error: invalid local address: %s", laddr); 
			goto udp_setup_error;
		}
		own_addr.sin_addr.s_addr = laddr_lin.s_addr;
	}
	own_addr.sin_len = sizeof(own_addr);

	if (ofp_bind(server_fd, (struct ofp_sockaddr *)&own_addr, sizeof(struct ofp_sockaddr)) < 0) 
	{
		OFPK_ERR("Error: ofp_bind failed, err='%s'", ofp_strerror(ofp_errno));
		goto udp_setup_error;
	}

	ofpk_server.server_fd       = server_fd;
	ofpk_server.laddr           = own_addr.sin_addr.s_addr;
	ofpk_server.lport           = own_addr.sin_port;
	ofpk_server.serverConStatus = IN_INIT;

	OFPK_INFO("udp server: %s:%d, server_fd: %d.\n",
				ofp_print_ip_addr(ofpk_server.laddr),
				odp_be_to_cpu_16(ofpk_server.lport),
				                 ofpk_server.server_fd);

	if(udp_epfd != 0)
	{
		struct ofp_epoll_event e = { OFP_EPOLLIN, { .fd = server_fd } };
		ofp_epoll_ctl(udp_epfd, OFP_EPOLL_CTL_ADD, server_fd , &e);
	}
	else
	{
		OFPK_ERR("Error: udp_epfd failed creat");
		goto udp_setup_error;
	}

	return OFPK_SUCC;
	
udp_setup_error:
	ofp_close(server_fd);
	return OFPK_ERROR;
}


/**
 * Setup tcp client
 */
int ofpk_setup_udp_client(char *daddr, uint16_t dport)
{
	int client_fd;
	struct in_addr laddr_lin;

	client_fd = ofp_socket(OFP_AF_INET, OFP_SOCK_DGRAM, OFP_IPPROTO_UDP);
	if (client_fd < 0) {		
		OFPK_ERR("Error: ofp_socket failed, err='%s'", ofp_strerror(ofp_errno));
		return OFPK_ERROR;
	}

	if (inet_aton(daddr, &laddr_lin) == 0) {
		OFPK_ERR("Error: invalid address: %s", daddr);
		return OFPK_ERROR;
	}

	ofpk_client.client_fd       = client_fd;
	ofpk_client.daddr           = laddr_lin.s_addr;
	ofpk_client.dport           = odp_cpu_to_be_16(dport);
	ofpk_client.clientConStatus = IN_INIT;

	//OFPK_INFO("\n udp client<%s:%d> setup ok fd:%d\n\n", daddr, dport, client_fd);

	OFPK_INFO("udp client: %s:%d, client_fd: %d.index:%d\n",
				ofp_print_ip_addr(ofpk_client.daddr),
				odp_be_to_cpu_16(ofpk_client.dport), 
								 ofpk_client.client_fd);
	
	return OFPK_SUCC;
}

static int ofpk_handle_udp_connection(int client_fd)
{
	
#if 0
	if (ofpk_server.connect_client[client_index].client_fd == 0)
	{
		OFPK_ERR("\nClient client_index<%d>\n",client_index);
		return OFPK_SUCC;
	}

	client_fd = ofpk_server.connect_client[client_index].client_fd;
	
	OFPK_ERR("\nClient client_index<%d>,client_fd<%d>\n",client_index, client_fd);
#endif

	OFPK_ERR("\nClient ,client_fd<%d>\n", client_fd);
	char pkt_buf[SOCKET_TX_BUF_LEN];
	int ret  = 0;

	for(;;)
	{
		ret = ofp_recv(client_fd, pkt_buf, SOCKET_RX_BUF_LEN-1, 0);		
		if (ret < 0 && ofp_errno == OFP_EWOULDBLOCK)
		{
			continue;
		}
		
		if (ret < 0) {
			OFPK_ERR("Error: ofp_recv failed: %d, err=%s\n", ofp_errno, ofp_strerror(ofp_errno));
			break;
		}

		if(ret)
		{
		    ofpk_recv_bytes += ret;
			
			ofpk_recv_counts++;
			
			OFPK_INFO("ofp recv byte:%ld, recv_counts:%ld, recv_bytes:%ld\n", 
						ret, ofpk_recv_counts, ofpk_recv_bytes);
			
			printf("ofp recv byte:%i, recv_counts:%i, recv_bytes:%i\n", 
						ret, ofpk_recv_counts, ofpk_recv_bytes);
		}
		else
		{
			OFPK_ERR("\nClient disconnected\n");
			ofp_epoll_ctl(udp_epfd, OFP_EPOLL_CTL_DEL, client_fd, NULL);
			break;
		}
	}

	return OFPK_SUCC;
}

static int ofpk_add_udp_client(int server_fd, int client_fd)
{
	#if 0
	struct ofp_sockaddr_in addr = {0};
	ofp_socklen_t addr_len      = sizeof(addr);
	
	int i         = 0;

	
	client_fd = ofp_accept(server_fd, (struct ofp_sockaddr *)&addr, &addr_len);
	if (client_fd == -1) {
		OFPK_ERR("Faild to accept connection, err='%s'", ofp_strerror(ofp_errno));
		return OFPK_ERROR;
	}

	if (addr_len != sizeof(struct ofp_sockaddr_in)) {
		OFPK_ERR("Faild to accept: invalid address size %d\n",addr_len);
		ofp_close(client_fd);
		return OFPK_ERROR;
	}
	
	OFPK_INFO("client Address: %s, port: %d.\n",
				ofp_print_ip_addr(addr.sin_addr.s_addr),
				odp_be_to_cpu_16(addr.sin_port));



	for (i = 0; i < NUM_CONNECTIONS; i++)
		if (ofpk_server.connect_client[i].client_fd == 0)
			break;

	if (i >= NUM_CONNECTIONS) {
		OFPK_ERR("Node cannot accept new connections!");
		ofp_close(client_fd);
		return OFPK_ERROR;
	}
	
	OFPK_ERR("\nClient client_index<%d>,client_fd<%d>\n",server_fd, client_fd );
	
	struct ofp_epoll_event e = { OFP_EPOLLIN, { .u32 = client_fd } };
	ofp_epoll_ctl(udp_epfd, OFP_EPOLL_CTL_ADD, client_fd, &e);

	OFPK_INFO("SUCCESS.\n");
	#endif
	return OFPK_SUCC;
}


/**
 * Run tcp server thread use epoll
 */
int ofpk_run_udp_server(void *args )
{
	int i, j, r, serv_fd, client_fd;
	
	gbl_args_t *gbl_args = (gbl_args_t *)args;

	if(udp_epfd == 0)
	{
		OFPK_ERR("Error: use epoll run tcp server");
		return OFPK_ERROR;
	}
	
	//uint8_t pkt_buf[SOCKET_RX_BUF_LEN] = {0};
	
	while(!exit_threads)
	{		
		struct ofp_epoll_event events[NUM_CONNECTIONS];
		
		r = ofp_epoll_wait(udp_epfd, events, NUM_CONNECTIONS, 0);

		for (i = 0; i < r; ++i) {
		
			client_fd = events[i].data.fd;
			
			OFPK_ERR("epoll wait client_fd:%d",client_fd);
			ofpk_handle_udp_connection(events[i].data.fd);
		}
	}
	
	return OFPK_SUCC;
}

static int test_udp_send(void)
{
	
	struct ofp_sockaddr_in dest_addr = {0};
	
	const char pkt_buf[] = "socket_test 123456789  0987654321  0123456789\n";
	int buf_len          = strlen(pkt_buf);
	
	dest_addr.sin_len         = sizeof(struct ofp_sockaddr_in);
	dest_addr.sin_family      = OFP_AF_INET;
	dest_addr.sin_port        = ofpk_client.dport;
	dest_addr.sin_addr.s_addr = ofpk_client.daddr;
	
	OFPK_INFO("\n\npkt_buf(%s),buf_len(%i)\n\n",pkt_buf, buf_len);
	
	OFPK_INFO("\n\n client send to server(%s:%i)",
					ofp_print_ip_addr(ofpk_client.daddr), 
					odp_be_to_cpu_16(ofpk_client.dport));

	int ret = 0;
	
	for(;;)
	{
		ret = ofp_sendto(ofpk_client.client_fd, pkt_buf, buf_len, 0,
						 (struct ofp_sockaddr *)&dest_addr, sizeof(dest_addr));			
		if (odp_unlikely(ret < 0)) {
			OFPK_ERR("Faild to send, err='%s'", ofp_strerror(ofp_errno));
			return OFPK_ERROR;
		}

		OFPK_INFO("ret(%i)\n",ret);

		ofpk_send_bytes += buf_len;

		ofpk_send_calls++;

		OFPK_INFO("client send to server(byte=%i, send_bytes=%i, send_calls=%i)\n\n",
					ret, ofpk_send_bytes, ofpk_send_calls);

		/* NOP unless OFP_PKT_TX_BURST_SIZE > 1 */
		//ofp_send_pending_pkt();
		
		sleep(5);
	}
	
	OFPK_INFO("\nServer disconnected\n\n");

	return OFPK_SUCC;
}


/**
 * Run tcp client
 */
int ofpk_run_udp_client(void *args)
{
	struct ofp_sockaddr_in addr;
	int ret = -1;
	
	gbl_args_t *gbl_args = (gbl_args_t *)args;
	
	ofpk_client.lport = odp_be_to_cpu_16(gbl_args->appl_args.lport);
	ofpk_client.laddr = inet_addr(gbl_args->appl_args.laddr);
		
	OFPK_INFO("Udp Client thread starting on CPU: %i\n", odp_cpu_id());
	
	if (ofpk_client.client_fd == 0)
	{
		OFPK_ERR("client setup error\n");	
		return OFPK_ERROR;
	}
		

	OFPK_ERR("client_fd connect (%d)\n", ofpk_client.client_fd);
		
	test_udp_send();

	return OFPK_SUCC;
}


