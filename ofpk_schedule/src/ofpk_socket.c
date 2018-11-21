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

/*----------------------------------------------*
* 外部变量说明                           		        *
*----------------------------------------------*/

/*----------------------------------------------*
* 内部函数原型说明                     			        *
*----------------------------------------------*/

/*----------------------------------------------*
* 外部函数原型说明                             			*
*----------------------------------------------*/



/* Sending function with some debugging. */
static int ofpk_send(int fd, char *package, int len)
{
	int n;
	
	while (len > 0) {
		n = ofp_send(fd, package, len, 0);
		if (n < 0) {
			OFPK_ERR("ofp_send failed n=%d, err='%s'", n, ofp_strerror(ofp_errno));
			return n;
		}
		len -= n;
		package += n;
		if (len) {
			OFPK_WARN("Only %d bytes sent", n);
		}
	}
	
	return len;
}



