/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_bus_vdev.h>
#include <rte_service_component.h>
#include <rte_flow.h>
#include <rte_version.h>
#include <nt.h>

#include "ntacc_eventdev.h"
#include "rte_eth_ntacc.h"
#include "rte_ntacc_event.h"

static int first = 0;
static int deviceCount = 0;

extern struct supportedDriver_s supportedDriver;
extern struct supportedAdapters_s supportedAdapters[NB_SUPPORTED_FPGAS];

static void *_libnt;

/* NTAPI library functions */
int (*_NT_Init)(uint32_t);
int (*_NT_NetRxOpen)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t, int);
int (*_NT_NetRxGet)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxRelease)(NtNetStreamRx_t, NtNetBuf_t);
int (*_NT_NetRxClose)(NtNetStreamRx_t);
char *(*_NT_ExplainError)(int, char *, uint32_t);
int (*_NT_NetTxOpen)(NtNetStreamTx_t *, const char *, uint64_t, uint32_t, uint32_t);
int (*_NT_NetTxClose)(NtNetStreamTx_t);
int (*_NT_InfoOpen)(NtInfoStream_t *, const char *);
int (*_NT_InfoRead)(NtInfoStream_t, NtInfo_t *);
int (*_NT_InfoClose)(NtInfoStream_t);
int (*_NT_ConfigOpen)(NtConfigStream_t *, const char *);
int (*_NT_ConfigClose)(NtConfigStream_t);
int (*_NT_NTPL)(NtConfigStream_t, const char *, NtNtplInfo_t *, uint32_t);
int (*_NT_NetRxGetNextPacket)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxOpenMulti)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t *, unsigned int, int);
int (*_NT_NetTxRelease)(NtNetStreamTx_t, NtNetBuf_t);
int (*_NT_NetTxGet)(NtNetStreamTx_t, NtNetBuf_t *, uint32_t, size_t, enum NtNetTxPacketOption_e, int);
int (*_NT_NetTxAddPacket)(NtNetStreamTx_t hStream, uint32_t port, NtNetTxFragment_t *fragments, uint32_t fragmentCount, int timeout );
int (*_NT_NetTxRead)(NtNetStreamTx_t hStream, NtNetTx_t *cmd);
int (*_NT_StatClose)(NtStatStream_t);
int (*_NT_StatOpen)(NtStatStream_t *, const char *);
int (*_NT_StatRead)(NtStatStream_t, NtStatistics_t *);
int (*_NT_NetRxRead)(NtNetStreamRx_t, NtNetRx_t *);
void (*_NT_FlowOpenAttrInit)(NtFlowAttr_t *);
void (*_NT_FlowOpenAttrSetAdapterNo)(NtFlowAttr_t *, uint8_t);
int (*_NT_FlowOpen_Attr)(NtFlowStream_t *, const char *, NtFlowAttr_t *);
int (*_NT_FlowClose)(NtFlowStream_t);
int (*_NT_FlowWrite)(NtFlowStream_t, NtFlow_t *, uint32_t);
int (*_NT_FlowRead)(NtFlowStream_t, NtFlowInfo_t*, uint32_t);
int (*_NT_FlowStatusRead)(NtFlowStream_t, NtFlowStatus_t*);
//#define ASSERT_CONCAT_(a, b) a##b
//#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
//#define ct_assert(e) enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }
#define ct_assert(e) 

static uint16_t
ntacc_eventdev_dequeue(void *port, struct rte_event *ev,
                       uint64_t timeout_ticks)
{
  struct ntacc_port *sp = port;
  uint16_t got_nb_events = 0;

  ct_assert(sizeof(eventFlowData_t) == sizeof(NtFlowInfo_t));
  ct_assert(sizeof(eventFlowStatusData_t) == sizeof(NtFlowStatus_t));
  ct_assert(sizeof(ev->u64) == sizeof(NtFlowStatus_t));

  if (sp->rxq->hFlowStream == NULL) {
    printf("The flow stream has been closed\n");
    return 0;
  }

  RTE_SET_USED(timeout_ticks);

  switch (ev->event) {
  case GET_FLOW_DATA:
    {
      ev->event_ptr = (void *)rte_malloc("event_ntacc", sizeof(NtFlowInfo_t), 0);
      if (ev->event_ptr) {
        if ((*_NT_FlowRead)(sp->rxq->hFlowStream, ev->event_ptr, 0) != NT_SUCCESS) {
          rte_free(ev->event_ptr);
          ev->event_ptr = NULL;
          return got_nb_events;
        }
        got_nb_events = 1;
      }
    }
    break;
  case GET_FLOW_STATUS:
    {
      ev->event_ptr = NULL;
      if ((*_NT_FlowStatusRead)(sp->rxq->hFlowStream, (NtFlowStatus_t*)&ev->u64) != NT_SUCCESS) {
        return got_nb_events;
      }
      got_nb_events = 1;
    }
    break;
  }
  return got_nb_events;
}

static uint16_t
ntacc_eventdev_dequeue_burst(void *port, struct rte_event ev[],
                             uint16_t nb_events, uint64_t timeout_ticks)
{
  struct ntacc_port *sp = port;
  uint16_t got_nb_events = 0;
  unsigned i;

  ct_assert(sizeof(eventFlowData_t) == sizeof(NtFlowInfo_t));
  ct_assert(sizeof(eventFlowStatusData_t) == sizeof(NtFlowStatus_t));
  ct_assert(sizeof(ev[0].u64) == sizeof(NtFlowStatus_t));
  
  if (sp->rxq->hFlowStream == NULL || nb_events == 0) {
    printf("The flow stream has been closed\n");
    return 0;
  }
  
  RTE_SET_USED(timeout_ticks);

  switch (ev[0].event) {
  case GET_FLOW_DATA:
    for (i = 0; i < nb_events; i++) {
      ev[i].event_ptr = (void *)rte_malloc("event_ntacc", sizeof(NtFlowInfo_t), 0);
      if (ev[i].event_ptr) {
        if ((*_NT_FlowRead)(sp->rxq->hFlowStream, ev[i].event_ptr, 0) != NT_SUCCESS) {
          rte_free(ev[i].event_ptr);
          ev[i].event_ptr = NULL;
          return got_nb_events;
        }
        got_nb_events++;
      }
    }
    break;
  case GET_FLOW_STATUS:
    for (i = 0; i < nb_events; i++) {
      ev[i].event_ptr = NULL;
      if ((*_NT_FlowStatusRead)(sp->rxq->hFlowStream, (NtFlowStatus_t*)&ev[i].u64) != NT_SUCCESS) {
        return got_nb_events;
      }
      got_nb_events++;
    }
    break;
  }
	return got_nb_events;
}

static void
ntacc_eventdev_info_get(struct rte_eventdev *dev,
		struct rte_event_dev_info *dev_info)
{
	struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(ev_internals);

	dev_info->min_dequeue_timeout_ns = 1;
	dev_info->max_dequeue_timeout_ns = 10000;
	dev_info->dequeue_timeout_ns = 25;
	dev_info->max_event_queues = RTE_ETHDEV_QUEUE_STAT_CNTRS;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 8;
	dev_info->max_event_ports = RTE_ETHDEV_QUEUE_STAT_CNTRS;
	dev_info->max_event_port_dequeue_depth = 128;
	dev_info->max_event_port_enqueue_depth = 128;
	dev_info->max_num_events = (1ULL << 20);
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					                  RTE_EVENT_DEV_CAP_BURST_MODE |
					                  RTE_EVENT_DEV_CAP_EVENT_QOS;
  dev_info->driver_name = ev_internals->driverName;
}

static int
ntacc_eventdev_configure(const struct rte_eventdev *dev)
{
	struct rte_eventdev_data *data = dev->data;
	struct rte_event_dev_config *conf = &data->dev_conf;
	struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(conf);
	RTE_SET_USED(ev_internals);

	PMD_DRV_LOG(INFO, "Configured eventdev devid=%d", dev->data->dev_id);
	return 0;
}

static int
ntacc_eventdev_close(struct rte_eventdev *dev)
{
	struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);

	PMD_DRV_FUNC_TRACE();
  PMD_DRV_LOG(INFO, "Closing event ID %u (%u) on adapter %u\n", dev->data->dev_id, deviceCount, ev_internals->adapterNo);

  rte_free(ev_internals);
  rte_event_pmd_release(dev);

  deviceCount--;
  if (deviceCount == 0 && _libnt != NULL) {
    PMD_DRV_LOG(INFO, "Closing dyn lib\n");
    dlclose(_libnt);
  }
	return 0;
}

static void
ntacc_eventdev_port_release(void *port)
{
	struct ntacc_port *sp = port;
	PMD_DRV_FUNC_TRACE();

	rte_free(sp);
}

static int
ntacc_eventdev_port_setup(struct rte_eventdev *dev, uint8_t port_id,
				const struct rte_event_port_conf *port_conf)
{
	struct ntacc_port *sp;
	struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);

	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(ev_internals);
	RTE_SET_USED(port_conf);

	/* Free memory prior to re-allocation if needed */
	if (dev->data->ports[port_id] != NULL) {
		PMD_DRV_LOG(INFO, "Freeing memory prior to re-allocation %d", port_id);
		ntacc_eventdev_port_release(dev->data->ports[port_id]);
		dev->data->ports[port_id] = NULL;
	}

	/* Allocate event port memory */
	sp = rte_zmalloc_socket("eventdev port", sizeof(struct ntacc_port), RTE_CACHE_LINE_SIZE, dev->data->socket_id);
	if (sp == NULL) {
		PMD_DRV_ERR("Failed to allocate sp port_id=%d", port_id);
		return -ENOMEM;
	}

	sp->port_id = port_id;
  sp->linked = false;

	PMD_DRV_LOG(INFO, "[%d] sp=%p", port_id, sp);

	dev->data->ports[port_id] = sp;
	return 0;
}

static int
ntacc_eventdev_port_link(struct rte_eventdev *dev, void *port,
			const uint8_t queues[], const uint8_t priorities[],
			uint16_t nb_links)
{
	struct ntacc_port *sp = port;
  struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);

  PMD_DRV_FUNC_TRACE();
  RTE_SET_USED(priorities);

  PMD_DRV_LOG(INFO, "Device %u - linking queue %u to port %u", dev->data->dev_id, queues[0], sp->port_id);

  if (nb_links > 1) {
    PMD_DRV_ERR("Only allowed to link one port");
    return -ENOMEM;
  }

  if (sp->rxq != NULL) {
    PMD_DRV_ERR("Port %u already linked. Must be unlinked before linked", sp->port_id);
    return -ENOMEM;
  }

  if (queues[0] > (RTE_ETHDEV_QUEUE_STAT_CNTRS - 1)) {
    PMD_DRV_ERR("Queue %u out of range. Max queue number is %u", queues[0], RTE_ETHDEV_QUEUE_STAT_CNTRS - 1);
    return -ENOMEM;
  }

  if (ev_internals->rxq[queues[0]] == NULL) {
    PMD_DRV_ERR("Invalid queue %u specified", queues[0]);
    return -ENOMEM;
  }

  if (!ev_internals->rxq[queues[0]]->enabled) {
    PMD_DRV_ERR("Queue %u is not enabled", queues[0]);
    return -ENOMEM;
  }

  sp->rxq = ev_internals->rxq[queues[0]];
  sp->queue_id = queues[0];
  sp->linked = true;

	/* Linked all the queues */
	return (int)nb_links;
}

static int
ntacc_eventdev_port_unlink(struct rte_eventdev *dev, void *port,
				 uint8_t queues[], uint16_t nb_unlinks)
{
	struct ntacc_port *sp = port;
	PMD_DRV_FUNC_TRACE();

	RTE_SET_USED(dev);
	RTE_SET_USED(sp);
	RTE_SET_USED(queues);

  if (nb_unlinks > 1) {
    PMD_DRV_ERR("Only allowed to unlink one port");
    return -ENOMEM;
  }

  if (sp->linked && sp->queue_id != queues[0]) {
    PMD_DRV_ERR("Port %u is not linked to queue %u, but to queue %u", sp->port_id, queues[0], sp->queue_id);
    return -ENOMEM;
  }

  sp->rxq = NULL;
  sp->linked = false;

  /* Unlinked all the queues */
	return (int)nb_unlinks;

}

static int ntacc_eventdev_rx_adapter_caps_get(const struct rte_eventdev *dev,
                                              const struct rte_eth_dev *eth_dev,
                                              uint32_t *caps)
{
  struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);
  struct pmd_internals *po_internals = ntacc_pmd_priv(eth_dev);

  PMD_DRV_FUNC_TRACE();
  RTE_SET_USED(ev_internals);
  RTE_SET_USED(po_internals);

  *caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT;

  return 0;
}

static int ntacc_eventdev_rx_adapter_queue_add(const struct rte_eventdev *dev,
                                               const struct rte_eth_dev *eth_dev,
                                               int32_t rx_queue_id,
                                               const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
  struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);
  struct pmd_internals *po_internals = ntacc_pmd_priv(eth_dev);
  unsigned i;

  RTE_SET_USED(ev_internals);
  RTE_SET_USED(queue_conf);

  if (rx_queue_id != -1) {
    PMD_DRV_ERR("Only -1 is valid as RX queue");
    return -ENODEV;
  }

  struct ntacc_rx_queue *rx_q = po_internals->rxq;

  PMD_DRV_FUNC_TRACE();

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    printf("Queue enabled: %u - nport (%u,%u) - StreamID %u - %p\n", rx_q[i].enabled, rx_q[i].in_port, rx_q[i].local_port, rx_q[i].stream_id, rx_q[i].hFlowStream);
    ev_internals->rxq[i] = &rx_q[i];
  }
  return 0;
}

static int ntacc_eventdev_rx_adapter_queue_del(const struct rte_eventdev *dev,
                                               const struct rte_eth_dev *eth_dev,
                                               int32_t rx_queue_id)
{
  struct ntacc_eventdev *ev_internals = ntacc_evt_priv(dev);
  unsigned i;

  PMD_DRV_FUNC_TRACE();
  RTE_SET_USED(eth_dev);

  if (rx_queue_id != -1) {
    PMD_DRV_ERR("Only -1 is valid as RX queue");
    return -ENODEV;
  }

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    ev_internals->rxq[i] = NULL;
  }
  return 0;
}

static int ntacc_eventdev_rx_adapter_start(const struct rte_eventdev *dev,
                                           const struct rte_eth_dev *eth_dev)
{
  RTE_SET_USED(dev);
  RTE_SET_USED(eth_dev);
  return 0;
}

static int ntacc_eventdev_rx_adapter_stop(const struct rte_eventdev *dev,
                                          const struct rte_eth_dev *eth_dev)
{
  RTE_SET_USED(dev);
  RTE_SET_USED(eth_dev);
  return 0;
}

/* Initialize and register event driver with DPDK Application */
static struct rte_eventdev_ops ntacc_eventdev_ops = {
	.dev_infos_get    = ntacc_eventdev_info_get,
	.dev_configure    = ntacc_eventdev_configure,
	.dev_close        = ntacc_eventdev_close,
	.port_setup       = ntacc_eventdev_port_setup,
	.port_release     = ntacc_eventdev_port_release,
	.port_link        = ntacc_eventdev_port_link,
	.port_unlink      = ntacc_eventdev_port_unlink,

  .eth_rx_adapter_start       = ntacc_eventdev_rx_adapter_start,
  .eth_rx_adapter_stop        = ntacc_eventdev_rx_adapter_stop,
  .eth_rx_adapter_caps_get    = ntacc_eventdev_rx_adapter_caps_get,
  .eth_rx_adapter_queue_add   = ntacc_eventdev_rx_adapter_queue_add,
  .eth_rx_adapter_queue_del   = ntacc_eventdev_rx_adapter_queue_del,
};

static int
ntacc_eventdev_init(struct rte_eventdev *eventdev, uint8_t adapterNo, uint8_t portNo, char *name)
{
	struct ntacc_eventdev *ev_internals = ntacc_evt_priv(eventdev);
	int ret = 0;

	PMD_DRV_FUNC_TRACE();

	eventdev->dev_ops       = &ntacc_eventdev_ops;
	eventdev->dequeue_burst = ntacc_eventdev_dequeue_burst;
  eventdev->dequeue       = ntacc_eventdev_dequeue;


  strcpy(ev_internals->name, name);
  strcpy(ev_internals->driverName, NTACC_EVENTDEV_NAME);
  ev_internals->adapterNo = adapterNo;
  ev_internals->portNo = portNo;

	PMD_DRV_LOG(INFO, "dev_id=%d socket_id=%d", eventdev->data->dev_id, eventdev->data->socket_id);

  return ret;
}

static int _nt_lib_open(void)
{
  char path[128];
  strcpy(path, NAPATECH3_LIB_PATH);
  strcat(path, "/libntapi.so");

  /* Load the library */
  _libnt = dlopen(path, RTLD_NOW);
  if (_libnt == NULL) {
    /* Library does not exist. */
    fprintf(stderr, "Failed to find needed library : %s\n", path);
    return -1;
  }
  _NT_Init = dlsym(_libnt, "NT_Init");
  if (_NT_Init == NULL) {
    fprintf(stderr, "Failed to find \"NT_Init\" in %s\n", path);
    return -1;
  }

  _NT_ConfigOpen = dlsym(_libnt, "NT_ConfigOpen");
  if (_NT_ConfigOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigOpen\" in %s\n", path);
    return -1;
  }
  _NT_ConfigClose = dlsym(_libnt, "NT_ConfigClose");
  if (_NT_ConfigClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigClose\" in %s\n", path);
    return -1;
  }
  _NT_NTPL = dlsym(_libnt, "NT_NTPL");
  if (_NT_NTPL == NULL) {
    fprintf(stderr, "Failed to find \"NT_NTPL\" in %s\n", path);
    return -1;
  }

  _NT_InfoOpen = dlsym(_libnt, "NT_InfoOpen");
  if (_NT_InfoOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoOpen\" in %s\n", path);
    return -1;
  }
  _NT_InfoRead = dlsym(_libnt, "NT_InfoRead");
  if (_NT_InfoRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoRead\" in %s\n", path);
    return -1;
  }
  _NT_InfoClose = dlsym(_libnt, "NT_InfoClose");
  if (_NT_InfoClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoClose\" in %s\n", path);
    return -1;
  }
  _NT_StatClose = dlsym(_libnt, "NT_StatClose");
  if (_NT_StatClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatClose\" in %s\n", path);
    return -1;
  }
  _NT_StatOpen = dlsym(_libnt, "NT_StatOpen");
  if (_NT_StatOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatOpen\" in %s\n", path);
    return -1;
  }
  _NT_StatRead = dlsym(_libnt, "NT_StatRead");
  if (_NT_StatRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatRead\" in %s\n", path);
    return -1;
  }
  _NT_ExplainError = dlsym(_libnt, "NT_ExplainError");
  if (_NT_ExplainError == NULL) {
    fprintf(stderr, "Failed to find \"NT_ExplainError\" in %s\n", path);
    return -1;
  }
  _NT_NetTxOpen = dlsym(_libnt, "NT_NetTxOpen");
  if (_NT_NetTxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetTxClose = dlsym(_libnt, "NT_NetTxClose");
  if (_NT_NetTxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpen = dlsym(_libnt, "NT_NetRxOpen");
  if (_NT_NetRxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetRxGet = dlsym(_libnt, "NT_NetRxGet");
  if (_NT_NetRxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGet\" in %s\n", path);
    return -1;
  }
  _NT_NetRxRelease = dlsym(_libnt, "NT_NetRxRelease");
  if (_NT_NetRxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxRelease\" in %s\n", path);
    return -1;
  }
  _NT_NetRxClose = dlsym(_libnt, "NT_NetRxClose");
  if (_NT_NetRxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxGetNextPacket = dlsym(_libnt, "NT_NetRxGetNextPacket");
  if (_NT_NetRxGetNextPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGetNextPacket\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpenMulti = dlsym(_libnt, "NT_NetRxOpenMulti");
  if (_NT_NetRxOpenMulti == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpenMulti\" in %s\n", path);
    return -1;
  }

  _NT_NetTxRelease = dlsym(_libnt, "NT_NetTxRelease");
  if (_NT_NetTxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxRelease\" in %s\n", path);
    return -1;
  }

  _NT_NetTxAddPacket = dlsym(_libnt, "NT_NetTxAddPacket");
  if (_NT_NetTxAddPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxAddPacket\" in %s\n", path);
    return -1;
  }
  _NT_NetTxRead = dlsym(_libnt, "NT_NetTxRead");
  if (_NT_NetTxRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxRead\" in %s\n", path);
    return -1;
  }
  _NT_NetTxGet = dlsym(_libnt, "NT_NetTxGet");
  if (_NT_NetTxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxGet\" in %s\n", path);
    return -1;
  }
  _NT_NetRxRead = dlsym(_libnt, "NT_NetRxRead");
  if (_NT_NetRxRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxRead\" in %s\n", path);
    return -1;
  }
  _NT_FlowOpenAttrInit = dlsym(_libnt, "NT_FlowOpenAttrInit");
  if (_NT_FlowOpenAttrInit == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowOpenAttrInit\" in %s\n", path);
    return -1;
  }
  _NT_FlowOpenAttrSetAdapterNo = dlsym(_libnt, "NT_FlowOpenAttrSetAdapterNo");
  if (_NT_FlowOpenAttrSetAdapterNo == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowOpenAttrSetAdapterNo\" in %s\n", path);
    return -1;
  }
  _NT_FlowOpen_Attr = dlsym(_libnt, "NT_FlowOpen_Attr");
  if (_NT_FlowOpen_Attr == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowOpen_Attr\" in %s\n", path);
    return -1;
  }
  _NT_FlowClose = dlsym(_libnt, "NT_FlowClose");
  if (_NT_FlowClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowClose\" in %s\n", path);
    return -1;
  }
  _NT_FlowWrite = dlsym(_libnt, "NT_FlowWrite");
  if (_NT_FlowWrite == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowWrite\" in %s\n", path);
    return -1;
  }
  _NT_FlowRead = dlsym(_libnt, "NT_FlowRead");
  if (_NT_FlowRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowRead\" in %s\n", path);
    return -1;
  }
  _NT_FlowStatusRead = dlsym(_libnt, "NT_FlowStatusRead");
  if (_NT_FlowStatusRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_FlowStatusRead\" in %s\n", path);
    return -1;
  }
  return 0;
}

static int ntacc_eventdev_probe(struct rte_vdev_device *dev)
{
  NtInfoStream_t hInfo = NULL;
  struct rte_eventdev *eventdev = NULL;
  char name[NTACC_NAME_LEN];
  NtInfo_t *pInfo = NULL;
  uint8_t nbPortsOnAdapter = 0;
  uint8_t nbAdapters = 0;
  uint8_t adapterNo = 0;
  uint8_t offset = 0;
  uint8_t localPort = 0;
  struct version_s version;
  int status;
  int iRet = 0;
  uint8_t i;
  bool found = false;

  PMD_DRV_FUNC_TRACE();

  PMD_DRV_LOG(INFO, "Initializing event_ntacc: %s %s\n", dev->device.name, rte_version());

  if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
    PMD_DRV_ERR("event_ntacc must run as a primary process");
    return -1;
  }

  if (first == 0) {
    status = _nt_lib_open();
    if (status < 0)
      return -1;

    (*_NT_Init)(NTAPI_VERSION);
    first++;
  }

  pInfo = (NtInfo_t *)rte_malloc("event_ntacc", sizeof(NtInfo_t), 0);
  if (!pInfo) {
    rte_panic("Cannot allocate memory for");
  }

  /* Open the information stream */
  if ((status = (*_NT_InfoOpen)(&hInfo, "DPDK Info stream")) != NT_SUCCESS) {
    PMD_DRV_ERR("NT_InfoOpen failed");
    iRet = status;
    goto error;
  }

  /* Find driver version */
  pInfo->cmd = NT_INFO_CMD_READ_SYSTEM;
  if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
    PMD_DRV_ERR("NT_InfoRead failed");
    iRet = status;
    goto error;
  }

  nbAdapters = pInfo->u.system.data.numAdapters;
  version.major = pInfo->u.system.data.version.major;
  version.minor = pInfo->u.system.data.version.minor;
  version.patch = pInfo->u.system.data.version.patch;

  // Check that the driver is supported
  if (((supportedDriver.major * 100) + supportedDriver.minor) > ((version.major * 100) + version.minor)) {
    PMD_DRV_LOG(ERR, "ERROR: NT Driver version %d.%d.%d is not supported. The version must be %d.%d.%d or newer.\n",
            version.major, version.minor, version.patch,
            supportedDriver.major, supportedDriver.minor, supportedDriver.patch);
    iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
    goto error;
  }

  for (i = 0; i < nbAdapters; i++) {
    found = false;
    pInfo->cmd = NT_INFO_CMD_READ_ADAPTER_V6;
    pInfo->u.adapter_v6.adapterNo = i;
    if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
      PMD_DRV_ERR("NT_InfoRead failed");
      iRet = status;
      goto error;
    }

    nbPortsOnAdapter = pInfo->u.adapter_v6.data.numPorts;
    offset = pInfo->u.adapter_v6.data.portOffset;
    adapterNo = i;
    PMD_DRV_LOG(INFO, "Found: "PCI_PRI_FMT": Ports %u, Offset %u, Adapter %u\n", pInfo->u.adapter_v6.data.busid.s.bus,
                                                                                 pInfo->u.adapter_v6.data.busid.s.device,
                                                                                 pInfo->u.adapter_v6.data.busid.s.domain,
                                                                                 pInfo->u.adapter_v6.data.busid.s.function,
                                                                                 nbPortsOnAdapter,
                                                                                 offset,
                                                                                 adapterNo);

    // Check if FPGA is supported
    for (i = 0; i < NB_SUPPORTED_FPGAS; i++) {
      if (supportedAdapters[i].item == pInfo->u.adapter_v6.data.fpgaid.s.item &&
          supportedAdapters[i].product == pInfo->u.adapter_v6.data.fpgaid.s.product) {
        if (((supportedAdapters[i].ver * 100) + supportedAdapters[i].rev) >
            ((pInfo->u.adapter_v6.data.fpgaid.s.ver * 100) + pInfo->u.adapter_v6.data.fpgaid.s.rev)) {
          PMD_DRV_LOG(WARNING, "ERROR: NT adapter firmware %03d-%04d-%02d-%02d-%02d is not supported. "
                               "The firmware must be %03d-%04d-%02d-%02d-%02d.\n",
                              pInfo->u.adapter_v6.data.fpgaid.s.item,
                              pInfo->u.adapter_v6.data.fpgaid.s.product,
                              pInfo->u.adapter_v6.data.fpgaid.s.ver,
                              pInfo->u.adapter_v6.data.fpgaid.s.rev,
                              pInfo->u.adapter_v6.data.fpgaid.s.build,
                              supportedAdapters[i].item,
                              supportedAdapters[i].product,
                              supportedAdapters[i].ver,
                              supportedAdapters[i].rev,
                              supportedAdapters[i].build);
          break;
        }
        found = true;
      }
    }

    if (!found) {
      continue;
    }

    for (localPort = 0; localPort < nbPortsOnAdapter; localPort++) {
      pInfo->cmd = NT_INFO_CMD_READ_PORT_V7;
      pInfo->u.port_v7.portNo = (uint8_t)localPort + offset;
      if ((status = (*_NT_InfoRead)(hInfo, pInfo)) != 0) {
        PMD_DRV_ERR("NT_InfoRead failed");
        iRet = status;
        goto error;
      }

      snprintf(name, NTACC_NAME_LEN, PCI_PRI_FMT " Port %u", pInfo->u.port_v7.data.adapterInfo.busid.s.domain,
                                                             pInfo->u.port_v7.data.adapterInfo.busid.s.bus,
                                                             pInfo->u.port_v7.data.adapterInfo.busid.s.device,
                                                             pInfo->u.port_v7.data.adapterInfo.busid.s.function,
                                                             localPort);

      PMD_DRV_LOG(INFO, "Port: %u - %s\n", offset + localPort, name);

    	eventdev = rte_event_pmd_vdev_init(name, sizeof(struct ntacc_eventdev), rte_socket_id());
      if (eventdev == NULL) {
        iRet = -ENOMEM;
        goto error;
      }

      if (ntacc_eventdev_init(eventdev, pInfo->u.port_v7.data.adapterNo, pInfo->u.port_v7.portNo, name) != 0) {
        iRet = 1;
        goto error;
      }
      deviceCount++;
    }
  }

  rte_free(pInfo);
  return iRet;

error:
  if (pInfo) {
    rte_free(pInfo);
  }
  if (hInfo)
    (void)(*_NT_InfoClose)(hInfo);
  if (eventdev->data->dev_private)
    rte_free(eventdev->data->dev_private);

  if (eventdev) {
    rte_event_pmd_release(eventdev);
  }

	RTE_EDEV_LOG_ERR("driver %s: failed", name);

	return -ENXIO;
}

static struct rte_vdev_driver vdev_eventdev_ntacc_pmd = {
	.probe = ntacc_eventdev_probe,
};

RTE_PMD_REGISTER_VDEV(event_ntacc, vdev_eventdev_ntacc_pmd);


