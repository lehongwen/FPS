/* Copyright (c) 2013-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_SHM_INTERNAL_H_
#define ODP_SHM_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/shared_memory.h>

#define SHM_DEVNAME_MAXLEN (ODP_SHM_NAME_LEN + 16)
#define SHM_DEVNAME_FORMAT "/odp-%d-%s" /* /dev/shm/odp-<pid>-<name> */

#define _ODP_SHM_PROC_NOCREAT 0x40  /**< Do not create shm if not exist */
#define _ODP_SHM_O_EXCL	      0x80  /**< Do not create shm if exist */

#ifdef __cplusplus
}
#endif

#endif
