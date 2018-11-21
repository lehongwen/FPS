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
#include <stdio.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>		

#include "ofpk.h"
#include "ofpk_paras.h"
#include "ofpk_socket.h"
#include "ofpk_common.h"
#include "ofpk_udp_fwd.h"

/*----------------------------------------------*
* 外部变量说明                           		        *
*----------------------------------------------*/

/*----------------------------------------------*
* 内部函数原型说明                     			        *
*----------------------------------------------*/

/*----------------------------------------------*
* 外部函数原型说明                             			*
*----------------------------------------------*/

/*----------------------------------------------*
* 内部数据                       			        *
*----------------------------------------------*/
#define APP_CORE 4

#define PKT_BURST_SIZE  OFP_PKT_RX_BURST_SIZE

static gbl_args_t gbl_args;

/** Break workers loop if set to 1 */
static int exit_threads;

/**
 * Signal handler for SIGINT
 */
static void sig_handler(int signo ODP_UNUSED)
{
	exit_threads = 1;
}

static int app_set_thread_name(pthread_t id, const char *format, ...)
{
	char thread_name[OFP_MAX_THREAD_NAME_LEN] = {0};
	
	int ret = -1;
	
	va_list argsList;

	va_start(argsList, format);
		
	/* Set thread_name for aid in debugging. */
	vsnprintf(thread_name, OFP_MAX_THREAD_NAME_LEN,format,argsList);

	va_end(argsList);
	
	ret = pthread_setname_np(id, thread_name);
	if (ret != 0)
	{
		OFP_ERR("Cannot set name for lcore thread\n");
		
		return OFPK_ERROR;
	}

	return OFPK_SUCC;
}

static void ofpk_odp_init(gbl_args_t *args)
{
	if (odp_init_global(&(args->instance), NULL, NULL)) {
		OFPK_ERR("Error: ODP global init failed\n");
		exit(EXIT_FAILURE);
	}
	
	if (odp_init_local(args->instance, ODP_THREAD_CONTROL)) {
		OFPK_ERR("Error: ODP local init failed\n");
		exit(EXIT_FAILURE);
	}
	
	OFPK_INFO("ofpk odp init succ\n");
}

static void ofpk_ofp_init(gbl_args_t *args)
{
	ofp_init_global_param(&(args->app_init_params));

	if (ofp_init_global(args->instance, &(args->app_init_params))) {
		OFPK_ERR("Error: OFP global init failed.\n");
		exit(EXIT_FAILURE);
	}
	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed.\n");
		exit(EXIT_FAILURE);
	}
	
	OFPK_INFO("ofpk ofp init succ\n");
}


static void ofpk_pktio_init(gbl_args_t *args)
{
	odp_pktio_param_init(&(args->pktio_param));
	args->pktio_param.in_mode  = ODP_PKTIN_MODE_DIRECT;
	args->pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;

	odp_pktin_queue_param_init(&args->pktin_param);
	args->pktin_param.op_mode     = ODP_PKTIO_OP_MT_UNSAFE;
	args->pktin_param.num_queues  = args->rx_queues; 
	args->pktin_param.hash_enable = 1;
	args->pktin_param.hash_proto.proto.ipv4_udp = 1; //指定协议类型udp

	odp_pktout_queue_param_init(&args->pktout_param);
	args->pktout_param.op_mode    = ODP_PKTIO_OP_MT_UNSAFE;
	args->pktout_param.num_queues = args->tx_queues;

	//memset(args->pktio_thr_args, 0, sizeof(pktio_thr_arg_t)*MAX_WORKERS);
	
	int i, j;
	
	odp_pktio_t pktio;
	
	for (i = 0; i < args->appl_args.if_count; i++) {
		
		odp_pktin_queue_t pktin[args->rx_queues];

		if (ofp_ifnet_create(args->instance, args->appl_args.if_names[i],
							 &args->pktio_param, &args->pktin_param, &args->pktout_param, 
							 args->app_init_params.if_mtu) < 0) {
			OFPK_ERR("Failed to init interface %s", args->appl_args.if_names[i]);
			exit(EXIT_FAILURE);
		}

		pktio = odp_pktio_lookup(args->appl_args.if_names[i]);
		if (pktio == ODP_PKTIO_INVALID) {
			OFPK_ERR("Failed locate pktio %s",args->appl_args.if_names[i]);
			exit(EXIT_FAILURE);
		}

		if (odp_pktin_queue(pktio, pktin, args->rx_queues) != args->rx_queues) {
			OFPK_ERR("Too few pktin queues for %s",args->appl_args.if_names[i]);
			exit(EXIT_FAILURE);
		}

		if (odp_pktout_queue(pktio, NULL, 0) != args->tx_queues) {
			OFPK_ERR("Too few pktout queues for %s",args->appl_args.if_names[i]);
			exit(EXIT_FAILURE);
		}

		for (j = 0; j < args->rx_queues; j++)
			args->pktio_thr_args[j].pktin[i] = pktin[j];
	}
	
	OFPK_INFO("ofpk pktio init succ\n");
}

/*
 * Should receive timeouts only
 */
static void *event_dispatcher(void *arg)
{
	odp_event_t ev;

	(void)arg;

	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed.\n");
		return NULL;
	}

	while (1) {
		ev = odp_schedule(NULL, ODP_SCHED_WAIT);

		if (ev == ODP_EVENT_INVALID)
			continue;

		if (odp_event_type(ev) == ODP_EVENT_TIMEOUT) {
			ofp_timer_handle(ev);
			continue;
		}

		OFPK_ERR("Error: unexpected event type: %u\n",  odp_event_type(ev));

		odp_buffer_free(odp_buffer_from_event(ev));
	}

	/* Never reached */
	return NULL;
}

static int event_dispatcher_task(gbl_args_t *args, int next_work)
{
	odph_linux_thr_params_t thr_params;
	
	odp_cpumask_t cpu_mask;
	odp_cpumask_zero(&cpu_mask);
	odp_cpumask_set(&cpu_mask, next_work);
	
	thr_params.start    = event_dispatcher;
	thr_params.arg      = NULL;
	
	thr_params.instance = args->instance;
	thr_params.thr_type = ODP_THREAD_WORKER;
	
	odph_linux_pthread_create(&args->event_thread, &cpu_mask, &thr_params);
	
	app_set_thread_name(args->event_thread.thread,"event_%d", next_work);
	
	++next_work;
	return (next_work);
}

static void *pkt_io_recv(void *arg)
{
	odp_packet_t pkt, pkt_tbl[PKT_BURST_SIZE];
	
	int pkt_idx, pkt_cnt;
	int num_pktin, i;
	
	pktio_thr_arg_t *thr_args;
	
	uint8_t *ptr;
	
	odp_pktin_queue_t pktin[OFP_FP_INTERFACE_MAX];

	thr_args  = arg;
	num_pktin = thr_args->num_pktin;

	for (i = 0; i < num_pktin; i++)
		pktin[i] = thr_args->pktin[i];

	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed.\n");
		return NULL;
	}
	ptr = (uint8_t *)&pktin[0];

	OFPK_INFO("PKT-IO receive starting on cpu: %i, num_pktin:%i,  ptr:  %x:%x\n", 
				odp_cpu_id(), num_pktin, ptr[0], ptr[8]);

	while (1) {
		for (i = 0; i < num_pktin; i++) {
			pkt_cnt = odp_pktin_recv(pktin[i], pkt_tbl,
						 PKT_BURST_SIZE);

			for (pkt_idx = 0; pkt_idx < pkt_cnt; pkt_idx++) {
				pkt = pkt_tbl[pkt_idx];

				ofp_packet_input(pkt, ODP_QUEUE_INVALID, ofp_eth_vlan_processing);
			}
		}
		ofp_send_pending_pkt();
	}

	/* Never reached */
	return NULL;
}

static int dataplane_dispatcher_task(gbl_args_t *args, int next_work)
{
	odph_linux_thr_params_t thr_params;
	
	odp_cpumask_t cpu_mask;
	
	int i = 0;
	memset(args->thread_tbl, 0, sizeof(args->thread_tbl));

	for (i = 0; i < args->rx_queues; ++i) {

		args->pktio_thr_args[i].num_pktin = args->appl_args.if_count;

		thr_params.start    = pkt_io_recv;
		thr_params.arg      = &args->pktio_thr_args[i];
		thr_params.instance = args->instance;
		
		thr_params.thr_type = ODP_THREAD_WORKER;

		odp_cpumask_zero(&cpu_mask);
		odp_cpumask_set(&cpu_mask, next_work);

		odph_linux_pthread_create(&args->thread_tbl[i], &cpu_mask, &thr_params);

		app_set_thread_name(args->thread_tbl[i].thread,"dataplane_%d", next_work);	
		
		++next_work;
	}
	
	return (next_work);
}


static int udp_fwd_task(gbl_args_t *args, int next_work)
{
	odph_linux_thr_params_t thr_params;
	
	odp_cpumask_t cpu_mask;
	odp_cpumask_zero(&cpu_mask);
	odp_cpumask_set(&cpu_mask, next_work);
	
	thr_params.start = udp_fwd_start;
	thr_params.arg   = args;
	
	thr_params.instance = args->instance;
	thr_params.thr_type = ODP_THREAD_WORKER;
	
	odph_linux_pthread_create(&args->app_thread, &cpu_mask, &thr_params);

	app_set_thread_name(args->app_thread.thread,"udpFwd_%d", next_work);
	
	++next_work;
	return (next_work);
}

/**
 * Run udp server
 */
static int run_udp_server(void *arg)
{
	OFPK_INFO("Server thread starting on CPU: %i\n", odp_cpu_id());

	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed\n");
		return OFPK_ERROR;
	}
	
	ofpk_run_udp_server(arg);	

	if (ofp_term_local())
		OFPK_ERR("Error: ofp_term_local failed\n");

	return OFPK_SUCC;
}


/**
 * Run udp client
 */
static int run_udp_client(void *arg)
{
	if (ofp_init_local()) {
		OFPK_ERR("Error: OFP local init failed\n");
		return OFPK_ERROR;
	}
	
	ofpk_run_udp_client(arg);	

	if (ofp_term_local())
		OFPK_ERR("Error: ofp_term_local failed\n");

	return OFPK_SUCC;
}

static int udp_server_task(gbl_args_t *args, int next_work)
{
 	odph_linux_thr_params_t thr_params;
	
	ofp_udp_localInit();
	
	if (ofpk_setup_udp_server(args->appl_args.laddr, args->appl_args.lport)) {
		OFPK_ERR("Error: failed to setup server,%s:%d\n",args->appl_args.laddr, args->appl_args.lport);
		exit(EXIT_FAILURE);
	}
	
	odp_cpumask_t cpu_mask;
	odp_cpumask_zero(&cpu_mask);
	odp_cpumask_set(&cpu_mask, next_work);
	
	thr_params.start = run_udp_server;
	thr_params.arg   = args;
	
	thr_params.instance = args->instance;
	thr_params.thr_type = ODP_THREAD_WORKER;
	
	odph_linux_pthread_create(&args->app_thread, &cpu_mask, &thr_params);
	
	app_set_thread_name(args->app_thread.thread,"udpServer_%d", next_work);
	++next_work;
	return (next_work);
}

static int udp_client_task(gbl_args_t *args, int next_work)
{
 	odph_linux_thr_params_t thr_params;
	
	ofp_udp_localInit();
	
	if (ofpk_setup_udp_client(args->appl_args.raddr, args->appl_args.rport)) {
		OFPK_ERR("Error: failed to setup client,%s:%d\n",args->appl_args.raddr, args->appl_args.rport);
		exit(EXIT_FAILURE);
	}
	
	odp_cpumask_t cpu_mask;
	odp_cpumask_zero(&cpu_mask);
	odp_cpumask_set(&cpu_mask, next_work);
	
	thr_params.start = run_udp_client;	
	thr_params.arg   = args;
	
	thr_params.instance = args->instance;
	thr_params.thr_type = ODP_THREAD_WORKER;
	
	odph_linux_pthread_create(&args->app_thread, &cpu_mask, &thr_params);
	
	app_set_thread_name(args->app_thread.thread,"udpClient_%d", next_work);
	
	++next_work;
	return (next_work);
}


int main(int argc, char *argv[])
{
	memset(&gbl_args, 0, sizeof(gbl_args_t));
	
	ofpk_odp_init(&gbl_args);

	/* Parse and store the application arguments */
	parse_args(argc, argv, &gbl_args.appl_args);

	/* Print both system and application information */
	print_info(NO_PATH(argv[0]), &gbl_args.appl_args);

	if (gbl_args.appl_args.if_count > OFP_FP_INTERFACE_MAX) {
		OFPK_ERR("Error: Invalid number of interfaces: maximum %d\n", OFP_FP_INTERFACE_MAX);
		exit(EXIT_FAILURE);
	}

	//signal(SIGINT, sig_handler);

	/*
	* By default core #0 runs Linux kernel background tasks. Start mapping
	* worker threads from core #1. Core #0 requires its own TX queue.
	*/
	int num_workers, first_worker, next_work, tbl_worker;
	int tx_queues, rx_queues;
	
	num_workers  = odp_cpu_count() - 1;  //cpu核数
	first_worker = FIRST_WORKER;
	tbl_worker   = 0;

	if (gbl_args.appl_args.core_count && gbl_args.appl_args.core_count < num_workers)
		num_workers = gbl_args.appl_args.core_count;	//io口创建任务个数 
		
	if (num_workers < 1) {
		OFPK_ERR("ERROR: At least 1 cores required.\n");
		exit(EXIT_FAILURE);
	}
	

	gbl_args.rx_queues = num_workers;
	gbl_args.tx_queues = ofp_min(num_workers, APP_CORE);
	
	OFPK_INFO("Num Worker threads:  %i",   num_workers);
	OFPK_INFO("First worker:    %i\n",     first_worker);
	OFPK_INFO("rx_queues:  %i, tx_queues: %i\n",  gbl_args.rx_queues, gbl_args.tx_queues);

	ofpk_ofp_init(&gbl_args);
	ofpk_pktio_init(&gbl_args);

	/* Start CLI */
	ofp_start_cli_thread(gbl_args.instance, gbl_args.app_init_params.linux_core_id, gbl_args.appl_args.conf_file);	
	sleep(2);

	next_work = event_dispatcher_task(&gbl_args, first_worker); //超时事件处理
	OFPK_INFO("event dispatcher task ok:  work(%i)\n",  next_work);
	
	next_work = dataplane_dispatcher_task(&gbl_args, next_work); //数据分发
	OFPK_INFO("dataplane dispatcher task ok:  work(%i)\n",  next_work);

	if(gbl_args.appl_args.mode == MODE_SERVER)
	{	
		gbl_args.appl_args.lport = TEST_SPORT;
		gbl_args.appl_args.rport = TEST_CPORT;
		next_work = udp_server_task(&gbl_args, next_work);// udp服务端
		OFPK_INFO("udp server task ok:	work(%i)\n",next_work);
	}
	else if(gbl_args.appl_args.mode == MODE_CLIENT)
	{
		gbl_args.appl_args.lport = TEST_CPORT;
		gbl_args.appl_args.rport = TEST_SPORT;
		next_work = udp_client_task(&gbl_args, next_work);// udp客户端
		OFPK_INFO("udp client task ok:	work(%i)\n",next_work);
	}
	else if(gbl_args.appl_args.mode == MODE_FWD)
	{
		gbl_args.appl_args.lport = TEST_SPORT;
		gbl_args.appl_args.rport = TEST_CPORT;
		next_work = udp_fwd_task(&gbl_args, next_work);// udp数据转发
		OFPK_INFO("udp fwd task ok:  work(%i)\n\n", next_work);
	}
	else
	{
		OFPK_ERR("ERROR: unkown run mod\n");
		exit(EXIT_FAILURE);
	}

	//odph_odpthreads_join(gbl_args.thread_tbl);
	odph_linux_pthread_join(gbl_args.thread_tbl, gbl_args.rx_queues);
	odph_linux_pthread_join(&gbl_args.app_thread, 1);

	OFPK_INFO("End Main()\n");
	
	return 0;
}
