/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include "config.h"

#include <stdarg.h>
#include <odp_api.h>
#include <odp_cunit_common.h>

/* flag set when the replacement logging function is used */
int replacement_logging_used;

/* replacement log function: */
ODP_PRINTF_FORMAT(2, 3)
static int odp_init_log(odp_log_level_t level __attribute__((unused)),
			const char *fmt, ...)
{
	va_list args;
	int r;

	/* just set a flag to be sure the replacement fn was used */
	replacement_logging_used = 1;

	va_start(args, fmt);
	r = vfprintf(stderr, fmt, args);
	va_end(args);

	return r;
}

/* test ODP global init, with alternate log function */
static void init_test_odp_init_global_replace_log(void)
{
	int status;
	odp_init_t init_data;
	odp_instance_t instance;

	odp_init_param_init(&init_data);
	init_data.log_fn = &odp_init_log;

	replacement_logging_used = 0;

	status = odp_init_global(&instance, &init_data, NULL);
	CU_ASSERT_FATAL(status == 0);

	CU_ASSERT_TRUE(replacement_logging_used || ODP_DEBUG_PRINT == 0);

	status = odp_term_global(instance);
	CU_ASSERT(status == 0);
}

odp_testinfo_t init_suite_log[] = {
	ODP_TEST_INFO(init_test_odp_init_global_replace_log),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t init_suites_log[] = {
	{"Init", NULL, NULL, init_suite_log},
	ODP_SUITE_INFO_NULL,
};

int main(int argc, char *argv[])
{
	int ret;

	/* parse common options: */
	if (odp_cunit_parse_options(argc, argv))
		return -1;

	/* prevent default ODP init: */
	odp_cunit_register_global_init(NULL);
	odp_cunit_register_global_term(NULL);

	/* register the tests: */
	ret = odp_cunit_register(init_suites_log);

	/* run the tests: */
	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
