/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
 
#ifndef __OSPK_SOCKET_H__
#define __OSPK_SOCKET_H__

/*----------------------------------------------*
 *宏定义                             *
 *----------------------------------------------*/
	/** Socket receive buffer length */
#define SOCKET_RX_BUF_LEN	(1 * 1024)
	
	/** Socket transmit buffer length */
#define SOCKET_TX_BUF_LEN	(1 * 1024)
	
/* Table of concurrent connections */
#define NUM_CONNECTIONS 16

/* Table of local socket number*/
#define NUM_SOCKETS     512

/** Break workers loop if set to 1 */
static int exit_threads = 0;

/*----------------------------------------------*
 *结构体定义                             *
 *----------------------------------------------*/


/**
 * Application modes
 */
typedef enum{
	UN_INIT = 0,
	IN_INIT,
	UN_CONNECT,	
	IN_CONNECT,
	UNKOWN
} connect_status_e;

typedef struct {
	int       			 client_fd;
	uint32_t  			 daddr;
	uint16_t 			 dport;
	connect_status_e     clientConStatus;   
}ofpk_connect_t;

typedef struct{
	int       				server_fd;
	uint32_t 			    laddr;
	uint16_t  				lport;
	connect_status_e    	serverConStatus;   
	ofpk_connect_t          connect_client[NUM_CONNECTIONS];  //服务端接入的客户端
}ofpk_server_t;     //服务端

typedef struct{
	int       				client_fd;
	uint32_t 			    laddr;  //客户端本地地址
	uint16_t  				lport;
	uint32_t  			    daddr;  //服务端地址
	uint16_t 			    dport;
	connect_status_e    	clientConStatus;   
}ofpk_client_t; //客户端


/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/

int ofpk_run_udp_server(void *args);
int ofpk_run_udp_client(void *args);

int ofpk_setup_udp_server(char *laddr, uint16_t lport);
int ofpk_setup_udp_client(char *daddr, uint16_t dport);

void ofp_udp_localInit(void);
void ofp_tcp_localInit(void);


#endif /*__OSPK_SOCKET_H__*/

