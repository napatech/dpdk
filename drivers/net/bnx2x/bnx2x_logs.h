/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * Copyright (c) 2015 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#ifndef _PMD_LOGS_H_
#define _PMD_LOGS_H_

#define PMD_INIT_LOG(level, sc, fmt, args...) \
	RTE_LOG(level, PMD, \
	"[bnx2x_pmd: %s] %s() " fmt "\n", (sc)->devinfo.name, __func__, ##args)

#ifdef RTE_LIBRTE_BNX2X_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE(sc) PMD_INIT_LOG(DEBUG, sc, " >>")
#else
#define PMD_INIT_FUNC_TRACE(sc) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG
#define PMD_DRV_LOG_RAW(level, sc, fmt, args...) \
	RTE_LOG(level, PMD, "[%s:%d(%s)] " fmt,	__func__, __LINE__, \
		(sc)->devinfo.name ? (sc)->devinfo.name : "", ## args)
#else
#define PMD_DRV_LOG_RAW(level, sc, fmt, args...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, sc, fmt, args...) \
	PMD_DRV_LOG_RAW(level, sc, fmt "\n", ## args)

#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
#define PMD_DEBUG_PERIODIC_LOG(level, sc, fmt, args...) \
	RTE_LOG(level, PMD, "%s(%s): " fmt "\n", __func__, \
		(sc)->devinfo.name ? (sc)->devinfo.name : "", ## args)
#else
#define PMD_DEBUG_PERIODIC_LOG(level, sc, fmt, args...) do { } while (0)
#endif


#endif /* _PMD_LOGS_H_ */
