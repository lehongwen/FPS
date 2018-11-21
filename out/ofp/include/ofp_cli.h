/* Copyright (c) 2014, ENEA Software AB
 * Copyright (c) 2014, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef __OFP_CLI_H__
#define __OFP_CLI_H__

#include <odp_api.h>

#if __GNUC__ >= 4
#pragma GCC visibility push(default)
#endif

/**
 * Start CLI server thread.
 *
 * Called by Application code to start the CLI server if needed.
 *
 * @param instance The ODP instance to use.
 * @param core_id The core on which the CLI server thread is started.
 * @param cli_file Name of the CLI file to process.
 * @retval 0 Success.
 * @retval -1 Failure.
 * @retval OFP_ENOTSUP OFP has been compiled without CLI support.
 */
int ofp_start_cli_thread(odp_instance_t instance, int core_id,
			 char *cli_file);

/**
 * Stop CLI server thread.
 *
 * @retval 0 Success.
 * @retval -1 Failure.
 * @retval OFP_ENOTSUP OFP has been compiled without CLI support.
 */
int ofp_stop_cli_thread(void);

/**
 * @page customCliCmd Customized CLI commands.
 *
 * CLI commands have the format
 * <pre>
 * keyword [keyword | arg]...
 * </pre>
 * where keyword is any string and arg is a placeholder for
 * one of the following:
 *
 * <pre>
 * Arg      Format
 * ---      ------
 * NUMBER   number
 * IP4ADDR  a.b.c.d
 * STRING   string
 * DEV      device name
 * IP4NET   a.b.c.d/n
 * IP6ADDR  a:b:c:d:e:f:g:h"
 * IP6NET   a:b:c:d:e:f:g:h/n"
 * </pre>
 *
 * Example: Add an IP address to an array position:
 *
 * @code
 * void my_func(void *handle, const char *args)
 * {
 *     // get socket
 *     int s =  ofp_cli_get_fd(handle);
 *     int pos;
 *     uint32_t addr;
 *     // args has format "10.20.30.4 5"
 *     [...parse args...]
 *
 *     if (my_array[pos] == 0) {
 *         send(s, "Pos not free!\r\n", 15, 0);
 *         return;
 *     }
 *
 *     my_array[pos] = addr;
 *     send(s, "OK\r\n", 4, 0);
 * }
 *
 * ofp_cli_add_command("add_ip_addr IP4ADDR to NUMBER",
 *                     "Add an IP address to a table position"
 *                     my_func);
 * @endcode
 *
 * Valid CLI command would be for example:
 * <pre>
 * add_ip_addr 10.20.30.4 to 5
 * </pre>
 */

/**
 * CLI callback function type.
 *
 * @param  handle  Use handle to get file descriptor.
 * @param  args    Command line arguments separated by a space.
 */
typedef void (*ofp_cli_cb_func)(void *handle, const char *args);

/**
 * Add a new CLI command. See @ref customCliCmd "customized CLI commands documentation".
 *
 * @param  cmd   Command line.
 * @param  help  Help text for the command.
 * @param  func  Function to call when CLI command is executed.
 */
void ofp_cli_add_command(const char *cmd, const char *help,
			 ofp_cli_cb_func func);

/**
 * Get file descriptor (socket) to write the response.
 *
 * @param  handle  Handle is the first argument of the callback function.
 *
 * @retval  File descriptor.
 */
int ofp_cli_get_fd(void *handle);

#if __GNUC__ >= 4
#pragma GCC visibility pop
#endif

#endif /* __OFP_CLI_H__ */
