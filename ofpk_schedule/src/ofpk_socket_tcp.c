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

static int init_local_flags = 0;
static int tcp_epfd         = 0;

/*----------------------------------------------*
* 外部函数原型说明                             			*
*----------------------------------------------*/

static  ofpk_server_t    ofpk_server;   //本地tcp服务端数据结构  
static  ofpk_client_t    ofpk_client;   //本地tcp客户端数据结构

static void ofp_tcp_localsInit(void)
{
	if(!init_local_flags)
	{
		memset(&ofpk_server, 0, sizeof( ofpk_server_t ));
		memset(&ofpk_client, 0, sizeof( ofpk_client_t ));
		tcp_epfd = ofp_epoll_create(1);
		init_local_flags = 1;
	}
}


/*****************************************************************************
  Function:        ofpk_setup_tcp_server
  Description:      
  Input:             
  Output:          NA
  Return:           
  Others:         
******************************************************************************/
int ofpk_setup_tcp_server(char *laddr, uint16_t lport)
{
	int server_fd     = 0;
	int ret        = -1;
	
	struct ofp_sockaddr_in own_addr = {0};

	server_fd = ofp_socket(OFP_AF_INET, OFP_SOCK_STREAM, OFP_IPPROTO_TCP);
	if (server_fd < 0) {
		OFPK_ERR("Error: ofp_socket failed\n");
		return OFPK_ERROR;
	}
	
	int optval = 1;
	ret = ofp_setsockopt(server_fd, OFP_SOL_SOCKET, OFP_SO_REUSEADDR, &optval, sizeof(optval));
	if (ret < 0) {
		OFPK_ERR("Error: ofp_setsockopt failed\n");
		goto tcp_setup_error;
	}
	ret = ofp_setsockopt(server_fd, OFP_SOL_SOCKET, OFP_SO_REUSEPORT, &optval, sizeof(optval));
	if (ret < 0) {
		OFPK_ERR("Error: ofp_setsockopt failed\n");
		goto tcp_setup_error;
	}

	memset(&own_addr, 0, sizeof(own_addr));
	own_addr.sin_len    = sizeof(struct ofp_sockaddr_in);
	own_addr.sin_family = OFP_AF_INET;
	own_addr.sin_port   = odp_cpu_to_be_16(lport);

	if (laddr == NULL) {
		ofp_setsockopt(server_fd, OFP_SOL_SOCKET, OFP_SO_BINDANY, &optval, sizeof(&optval));
		if (ret < 0) {
			OFPK_ERR("Error: ofp_setsockopt failed\n");
			goto tcp_setup_error;
		}
		own_addr.sin_addr.s_addr = OFP_INADDR_ANY;
	} else {
		struct in_addr laddr_lin;
		if (inet_aton(laddr, &laddr_lin) == 0) {
			OFPK_ERR("Error: invalid local address: %s", laddr); 
			goto tcp_setup_error;
		}
		own_addr.sin_addr.s_addr = laddr_lin.s_addr;
	}
	own_addr.sin_len = sizeof(own_addr);

	if (ofp_bind(server_fd, (struct ofp_sockaddr *)&own_addr, sizeof(struct ofp_sockaddr)) < 0) {
		OFPK_ERR("Error: ofp_bind failed, err='%s'", ofp_strerror(ofp_errno));
		goto tcp_setup_error;
	}

	if (ofp_listen(server_fd, TCP_SOCKET_BACKLOG)) {
		OFPK_ERR("Error: ofp_listen failed, err='%s'", ofp_strerror(ofp_errno));
		goto tcp_setup_error;
	}
	
	OFPK_INFO("\n tcp server<%s:%d> setup ok fd:%d\n\n", laddr,lport,server_fd);


	ofpk_server.server_fd       = server_fd;
	ofpk_server.laddr           = own_addr.sin_addr.s_addr;
	ofpk_server.lport           = odp_be_to_cpu_16(lport);
	ofpk_server.serverConStatus = IN_INIT;


	if(tcp_epfd != 0)
	{
		struct ofp_epoll_event e = { OFP_EPOLLIN, { .fd = server_fd } };
		ofp_epoll_ctl(tcp_epfd, OFP_EPOLL_CTL_ADD, server_fd , &e);
	}
	else
	{
		OFPK_ERR("Error: udp_epfd failed creat");
		goto tcp_setup_error;
	}

	return OFPK_SUCC;
	
tcp_setup_error:
	ofp_close(server_fd);
	return OFPK_ERROR;
}


/**
 * Setup tcp client
 */
int ofpk_setup_tcp_client(char *daddr, uint16_t dport)
{
	int client_fd;
	struct in_addr laddr_lin;

	client_fd = ofp_socket(OFP_AF_INET, OFP_SOCK_STREAM, OFP_IPPROTO_TCP);
	if (client_fd < 0) {
		OFPK_ERR("Error: ofp_socket failed\n");
		return OFPK_ERROR;
	}

	if (inet_aton(daddr, &laddr_lin) == 0) {
		OFPK_ERR("Error: invalid address: %s", daddr);
		return OFPK_ERROR;
	}

	ofpk_client.client_fd       = client_fd;
	ofpk_client.daddr           = laddr_lin.s_addr;
	ofpk_client.dport           = odp_be_to_cpu_16(dport);
	ofpk_client.clientConStatus = IN_INIT;

	OFPK_INFO("\n tcp clinet<%s:%d> setup ok fd:%d\n\n", daddr,dport,client_fd);
	return OFPK_SUCC;
}

static int ofpk_handle_tcp_connection(int client_index)
{
	int client_fd = -1;
	int ret       = -1;
	
	int recv_bytes, recv_counts;
	
	uint8_t pkt_buf[SOCKET_RX_BUF_LEN] = {0};
	
	client_fd = ofpk_server.connect_client[client_index].client_fd;
	
	OFPK_ERR("\nClient  fd<%d>\n",client_fd);

	for(;;)
	{
		ret = ofp_recv(client_fd, pkt_buf, SOCKET_RX_BUF_LEN-1, 0);		
		//ret = ofp_recv(tcp_client_connections[client_index].client_fd, pkt_buf, SOCKET_RX_BUF_LEN-1, OFP_MSG_NBIO);
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
		    recv_bytes += ret;
			recv_counts++;
			OFPK_INFO("ofp recv 1 client_fd:%d, current_byte:%d, recv_counts:%d, recv_bytes:%d\n", 
						client_fd,ret, recv_counts, recv_bytes);
		}
		else if(0 == ret)
		{
			OFPK_INFO("ofp recv 2 succ client_fd:%d, current_byte:%d, recv_counts:%d, recv_bytes:%d\n", 
						client_fd, ret, recv_counts, recv_bytes);
			return client_fd;
		}
		else
		{
			OFPK_ERR("\nClient disconnected\n");
			ofp_epoll_ctl(tcp_epfd, OFP_EPOLL_CTL_DEL, client_fd, NULL);
			break;
		}
	}

	return client_fd;
}

static int ofpk_accept_tcp_client(int server_fd)
{
	struct ofp_sockaddr_in addr = {0};
	ofp_socklen_t addr_len      = sizeof(addr);
	
	int client_fd = -1;
	int i         = 0;
	
	client_fd = ofp_accept(server_fd, (struct ofp_sockaddr *)&addr, &addr_len);
	if (client_fd == -1) {
		OFPK_ERR("Faild to accept connection: %d, err=%s\n", ofp_errno, ofp_strerror(ofp_errno));
		return OFPK_ERROR;
	}

	if (addr_len != sizeof(struct ofp_sockaddr_in)) {
		OFPK_ERR("Faild to accept: invalid address size %d\n",addr_len);
		ofp_close(client_fd);
		return OFPK_ERROR;
	}

	OFPK_INFO("new client Address: %s, port: %d.\n",
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
	
	struct ofp_timeval tv = {0};
	tv.tv_sec  = 3;
	tv.tv_usec = 0;
	
	int r2 = ofp_setsockopt(client_fd, OFP_SOL_SOCKET, OFP_SO_SNDTIMEO, &tv, sizeof tv);
	if (r2 !=0 ) OFPK_ERR("SO_SNDTIMEO failed!");

	ofpk_server.connect_client[i].client_fd        = client_fd;
	ofpk_server.connect_client[i].daddr            = addr.sin_addr.s_addr;
	ofpk_server.connect_client[i].dport            = addr.sin_port;
	ofpk_server.connect_client[i].clientConStatus = IN_CONNECT;


	OFPK_INFO("SUCCESS.\n");
	return client_fd;
}


/**
 * Run tcp server thread use epoll
 */
int ofpk_run_tcp_server(void)
{
	int i, r, serv_fd,client_fd;

	if(tcp_epfd == 0)
	{
		OFPK_ERR("Error: use epoll run tcp server");
		return OFPK_ERROR;
	}
	
	//uint8_t pkt_buf[SOCKET_RX_BUF_LEN] = {0};
	
	while(!exit_threads)
	{		
		struct ofp_epoll_event events[NUM_CONNECTIONS];
		
		r = ofp_epoll_wait(tcp_epfd, events, NUM_CONNECTIONS, 200);

		for (i = 0; i < r; ++i) {
		
			serv_fd = events[i].data.fd;

			if(ofpk_server.server_fd == serv_fd)
			{
				OFPK_INFO("\n tcp serv_fd<%d> sconnect new client\n",serv_fd );
				
				client_fd = ofpk_accept_tcp_client(serv_fd);
					
				struct ofp_epoll_event e = { OFP_EPOLLIN, { .u32 = i } };
				ofp_epoll_ctl(tcp_epfd, OFP_EPOLL_CTL_ADD, client_fd, &e);
				
				break;
			}
			else
			{
				OFPK_ERR("\nClient client_index<%d>\n",i);
				client_fd = ofpk_handle_tcp_connection(events[i].data.u32);
			}
		}
	}
}

static int test_tcp_send(void)
{
	int ret,retry;
	
	uint64_t recv_calls;	/**< Number of recv() function calls */
	uint64_t recv_bytes;	/**< Bytes received */
	uint64_t send_calls;	/**< Number of send() function calls */
	uint64_t send_bytes;	/**< Bytes sent */
	
	const char *pkt_buf = "socket_test 123456789  0987654321  0123456789";
	int buf_len         = strlen(pkt_buf);

	while(!exit_threads)
	{
		{
			ret = ofp_send(ofpk_client.client_fd, pkt_buf, buf_len, 0);
			
			if (odp_unlikely(ret < 0)) {
				OFPK_ERR("Failed to send (errno = %d)\n", ofp_errno);
				exit_threads = 1;
				break;
			}
			
			send_bytes += ret;
			send_calls++;
			
			OFPK_INFO("send client(%s:%i, fd:%d) (send_bytes=%i,send_calls=%i)\n",
						ofp_print_ip_addr(ofpk_client.daddr), 
						odp_be_to_cpu_16(ofpk_client.dport), 
						send_bytes,send_calls);

			/* NOP unless OFP_PKT_TX_BURST_SIZE > 1 */
			ofp_send_pending_pkt();

		}
		sleep(5);
	}
	
	OFPK_INFO("\nServer disconnected\n\n");

	return 0;
}

/**
 * Run tcp client
 */
int ofpk_run_tcp_client(void)
{
	struct ofp_sockaddr_in raddr;
	int i, ret = -1;
	
	OFPK_INFO("Tcp Client thread starting on CPU: %i\n", odp_cpu_id());

	memset(&raddr, 0, sizeof(struct ofp_sockaddr_in));
			
	raddr.sin_len		  = sizeof(struct ofp_sockaddr_in);
	raddr.sin_family 	  = OFP_AF_INET;
	raddr.sin_port		  = ofpk_client.dport;
	raddr.sin_addr.s_addr = ofpk_client.daddr;

	ret = ofp_connect(ofpk_client.client_fd, (struct ofp_sockaddr *)&raddr, sizeof(raddr));
	if ((ret == -1) && (ofp_errno != OFP_EINPROGRESS)) {
		OFPK_ERR("Error: Failed to connect: %d, err=%s\n", ofp_errno, ofp_strerror(ofp_errno));
		OFPK_INFO("\n tcp clinet<%s:%d> connect error\n", 
					ofp_print_ip_addr(raddr.sin_addr.s_addr),
					odp_be_to_cpu_16(raddr.sin_port));
		
		return OFPK_ERROR;
	}
	
	ofpk_client.clientConStatus = IN_CONNECT;
	
	sleep(2);
	
	test_tcp_send();

	return OFPK_SUCC;
}
