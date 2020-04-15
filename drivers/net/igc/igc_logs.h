/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_LOGS_H_
#define _IGC_LOGS_H_

#include <rte_log.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int igc_logtype_init;
extern int igc_logtype_driver;

#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, igc_logtype_init, \
		"%s(): " fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, igc_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#ifdef __cplusplus
}
#endif

#endif /* _IGC_LOGS_H_ */
