/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __NTACC_EVENTDEV_H__
#define __NTACC_EVENTDEV_H__

#include <rte_eventdev_pmd_pci.h>
#include <rte_eventdev_pmd_vdev.h>

#ifdef RTE_LIBRTE_PMD_NTACC_EVENTDEV_DEBUG
#define PMD_DRV_LOG(level, fmt, args...) RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
//#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(INFO, ">>")
#else
#define PMD_DRV_LOG(level, fmt, args...) do { } while (0)
#define PMD_DRV_FUNC_TRACE() do { } while (0)
#endif

#define PMD_DRV_ERR(fmt, args...) RTE_LOG(ERR, PMD, "%s(): " fmt "\n", __func__, ## args)

#define NTACC_NAME_LEN (PCI_PRI_STR_SIZE + 10)
#define SW_PMD_NAME_MAX 64

struct ntacc_events {
  uint32_t eventNumber;
};

enum rxp_service_state {
	SS_NO_SERVICE = 0,
	SS_REGISTERED,
	SS_READY,
	SS_RUNNING,
};

struct ntacc_eventdev {
	uintptr_t reg_base;
  struct ntacc_rx_queue *rxq[RTE_ETHDEV_QUEUE_STAT_CNTRS];

  uint8_t adapterNo;
  uint8_t portNo;

  char name[NTACC_NAME_LEN];
  char driverName[SW_PMD_NAME_MAX];
} __rte_cache_aligned;

struct ntacc_port {
	uint8_t port_id;
  uint8_t queue_id;
  bool linked;
  struct ntacc_rx_queue *rxq;
} __rte_cache_aligned;

static inline struct ntacc_eventdev *
ntacc_evt_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#endif /* __ntacc_EVENTDEV_H__ */
