/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
 
#ifndef __OSPK_H__
#define __OSPK_H__


#include "ofp.h"
#include "ofpk_paras.h"

/*----------------------------------------------*
 *宏定义                                           *
 *----------------------------------------------*/
		
#define FIRST_WORKER	1   //默认使用Core#1 作为首任何核
	
#define MAX_WORKERS	    64
	
	/* Maximum thread_name length. */
#define OFP_MAX_THREAD_NAME_LEN 16

	
#define OFPK_SUCC                (0)  
#define OFPK_ERROR               (-1)
#define OFPK_QUIT                (-2)

typedef uint8_t   U8;
typedef int8_t    S8;
typedef uint16_t  U16;
typedef uint32_t  U32;
typedef int32_t   S32;
typedef uint64_t  U64;
typedef int8_t    UBOOL;


#define __OFPK_FILENAME__ \
		(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

enum ofpk_log_level_s {
	OFPK_LOG_ERROR,
	OFPK_LOG_WARNING,
	OFPK_LOG_INFO,
	OFPK_LOG_DEBUG,
	OFPK_LOG_MAX_LEVEL
};

#define OFPK_LOG_LOCAL

#if defined (OFPK_LOG_LOCAL)
#define OFPK_LOG(level,fmt, ...)    do{\
		fprintf(stderr, "%s %d %d:%u %s:%d] " fmt "\n", 	\
			(level == OFPK_LOG_ERROR)   ? "E" :		\
			(level == OFPK_LOG_WARNING) ? "W" :		\
			(level == OFPK_LOG_INFO)    ? "I" :		\
			(level == OFPK_LOG_DEBUG)   ? "D" : "?", 	\
			ofp_timer_ticks(0), 			\
			odp_cpu_id(), (unsigned int) pthread_self(),	\
			__OFPK_FILENAME__, __LINE__, 			\
			##__VA_ARGS__);					\
	}while (0)
#else

#define OFPK_LOG(level,fmt, ...) 

//#define OFPK_LOG(logLevel,format, ...)  do {	   \
//		//cspl_log( (U32 )logLevel, (U32)0xFFAAAA,  __FILE__, __func__,(U32)__LINE__, \
//		//		  (const char*)format, ##__VA_ARGS__); \
//   } while (0)
#endif /*OFPK_LOG_LOCAL*/

#define OFPK_INFO(fmt, ...) \
	OFPK_LOG(OFPK_LOG_INFO, fmt, ##__VA_ARGS__)
#define OFPK_WARN(fmt, ...) \
	OFPK_LOG(OFPK_LOG_WARNING, fmt, ##__VA_ARGS__)
#define OFPK_ERR(fmt, ...) \
	OFPK_LOG(OFPK_LOG_ERROR, fmt, ##__VA_ARGS__)

#define ofpk_max(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

#define ofpk_min(x,y) ({ \
	const __typeof__(x) _x = (x);	\
	const __typeof__(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

/*----------------------------------------------*
 *结构体定义                                         *
 *----------------------------------------------*/


typedef struct{
	int               num_pktin;
	odp_pktin_queue_t pktin[OFP_FP_INTERFACE_MAX];
}pktio_thr_arg_t;


typedef struct{

	int 			         rx_queues;
	int  			         tx_queues;

	appl_args_t              appl_args;
	
	odp_instance_t     	     instance;	
	
 	odp_shm_t                shm;
	
	ofp_global_param_t       app_init_params; /**< global OFP init parms */
	
	odp_pktio_param_t        pktio_param;	
	pktio_thr_arg_t          pktio_thr_args[MAX_WORKERS];
	
	odp_pktin_queue_param_t  pktin_param;
	odp_pktout_queue_param_t pktout_param;

	odph_linux_pthread_t     thread_tbl[MAX_WORKERS];
	odph_linux_pthread_t     app_thread;
	odph_linux_pthread_t     event_thread;
}gbl_args_t;

/*----------------------------------------------*
 * 内部函数原型说明                                     *
 *----------------------------------------------*/

#endif /*__OSPK_H__*/

