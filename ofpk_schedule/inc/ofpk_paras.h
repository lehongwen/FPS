/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
 
#ifndef __OSPK_PARAS_H__
#define __OSPK_PARAS_H__

/*----------------------------------------------*
 *宏定义                             *
 *----------------------------------------------*/

#define TEST_SPORT 5001
#define TEST_CPORT 5000

/*----------------------------------------------*
 *结构体定义                             *
 *----------------------------------------------*/

/**
 * Application modes
 */
typedef enum appl_mode_t {
	MODE_SERVER = 0,
	MODE_CLIENT,	
	MODE_FWD,
	MODE_MUTI,
} appl_mode_t;


/**
* Parsed command line application arguments
*/
typedef struct {
	appl_mode_t mode;			/**< Application mode (client/server) */
	int 		core_count; 
	int 		if_count;		/**< Number of interfaces to use */
	int 		sock_count; 	/**< Number of sockets to udp fwd use */
	char	  **if_names;		/**< Array of pointers to interface names */
	char	   *conf_file;
	char	   *laddr;          /**< Number of laddr to udp use */
	uint16_t	lport;			/**< Listening port number */
	char	   *raddr;		    /**< Number of raddr to udp use */
	uint16_t	rport;			/**< Destination port number */
} appl_args_t;

/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/
void print_info(char *progname, appl_args_t *appl_args);
void parse_args(int argc, char *argv[], appl_args_t *appl_args);


#endif /*__OSPK_PARAS_H__*/

