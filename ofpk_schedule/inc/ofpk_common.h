/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
 
#ifndef __OSPK_COMMON_H__
#define __OSPK_COMMON_H__

/*----------------------------------------------*
 *宏定义                             *
 *----------------------------------------------*/
	
/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ? \
						    strrchr((file_name), '/') + 1 : (file_name))


/*----------------------------------------------*
 *结构体定义                             *
 *----------------------------------------------*/
    

/*----------------------------------------------*
 * 内部函数原型说明                             *
 *----------------------------------------------*/


#endif /*__OSPK_COMMON_H__*/

