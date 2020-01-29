/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_malloc.h>
#include <nt.h>

#include "rte_eth_ntacc.h"
#include "ntacc_shared.h"
#include "filter_keymatcher.h"
#include "keymatcher.h"

#define USE_KEY_MATCH

struct rte_flow *dev_flow_keymatcher_create(struct rte_eth_dev *dev,
                                            const struct rte_flow_attr *attr,
                                            const struct rte_flow_item items[],
                                            const struct rte_flow_action actions[],
                                            struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  uint32_t ntplID;
  uint8_t nb_queues = 0;
  uint8_t list_queues[256];
  bool filterContinue = false;
  const struct rte_flow_action_rss *rss = NULL;
  struct color_s color = {0, 0, false};
  uint8_t nb_ports = 0;
  uint8_t list_ports[MAX_NTACC_PORTS];
  uint8_t action = 0;
  uint8_t forwardPort;
  bool tunnel = false;

  uint64_t typeMask = 0;
  bool reuse = false;
  int key;
  int i;

  char *ntpl_buf = NULL;
  char *filter_buf1 = NULL;
  struct rte_flow *flow = NULL;
  const char *ntpl_str = NULL;

  // Init error struct
  rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_NONE, NULL, "No errors");

  filter_buf1 = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
  if (!filter_buf1) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }

  flow = rte_malloc(internals->name, sizeof(struct rte_flow), 0);
  if (!flow) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }
  memset(flow, 0, sizeof(struct rte_flow));

  NTACC_LOCK(&internals->configlock);

  if (handle_actions(dev,
                     actions,
                     &rss,
                     &forwardPort,
                     &typeMask,
                     &color,
                     &action,
                     &nb_queues,
                     list_queues,
                     internals,
                     error) != 0) {
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }

  if (handle_items(items,
                   &typeMask,
                   &tunnel,
                   &color,
                   &filterContinue,
                   &nb_ports,
                   list_ports,
                   &ntpl_str,
                   filter_buf1,
                   internals,
                   error) != 0) {
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }

  if (attr->group) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL, "Attribute groups are not supported");
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }
  if (attr->egress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL, "Attribute egress is not supported");
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }
  if (!attr->ingress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, NULL, "Attribute ingress must be set");
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }

  reuse = IsFilterReuseKeymatcher(internals, typeMask, list_queues, nb_queues, &key);
  if (!reuse) {
    ntpl_buf = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
    if (!ntpl_buf) {
      rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
      NTACC_UNLOCK(&internals->configlock);
      goto FlowError;
    }

    if (action & (ACTION_RSS | ACTION_QUEUE)) {
      // This is not a Drop filter or a retransmit filter
      if (nb_queues == 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "A queue must be defined");
        NTACC_UNLOCK(&internals->configlock);
        goto FlowError;
      }

      // Check the queues
      for (i = 0; i < nb_queues; i++) {
        if (!internals->rxq[list_queues[i]].enabled) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "All defined queues must be enabled");
          NTACC_UNLOCK(&internals->configlock);
          goto FlowError;
        }
      }

      switch (color.type)
      {
      case NO_COLOR:
    #ifdef COPY_OFFSET0
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=26,colorbits=14,Offset0=%s[0];", attr->priority, STRINGIZE_VALUE_OF(COPY_OFFSET0));
    #else
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=26,colorbits=14;", attr->priority);
    #endif
        break;
      case ONE_COLOR:
    #ifdef COPY_OFFSET0
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=22,colorbits=32,Offset0=%s[0];", attr->priority, STRINGIZE_VALUE_OF(COPY_OFFSET0));
    #else
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=22,colorbits=32;", attr->priority);
    #endif
        break;
      case COLOR_MASK:
        if (tunnel) {
          snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=24,colorbits=32,Offset0=InnerLayer3Header[0],Offset1=InnerLayer4Header[0];", attr->priority);
        }
        else {
          snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=24,colorbits=32,Offset0=Layer3Header[0],Offset1=Layer4Header[0];", attr->priority);
        }
        break;
      }
      // Set the stream IDs
      CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, nb_queues, list_queues);
    }
    else if (action & ACTION_DROP) {
      snprintf(ntpl_buf, NTPL_BSIZE, "assign[streamid=drop;priority=%u;", attr->priority);
      color.type = NO_COLOR;
    }
    else if (action & ACTION_FORWARD) {
      snprintf(ntpl_buf, NTPL_BSIZE, "assign[streamid=drop;priority=%u;DestinationPort=%u", attr->priority, forwardPort);
      color.type = NO_COLOR;
    }
    else {
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Queue, RSS, drop or forward information must be set");
      NTACC_UNLOCK(&internals->configlock);
      goto FlowError;
    }

    // Create HASH
    if (action == ACTION_RSS) {
      // If RSS is used, then set the Hash mode
      CreateHash(&ntpl_buf[strlen(ntpl_buf)], rss, internals);
    }

    // Set the color
    switch (color.type)
    {
    case ONE_COLOR:
      if (typeMask == 0) {
        // No values are used for any filter. Then we need to add color to the assign
        snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";color=%u", color.color);
      }
      break;
    case COLOR_MASK:
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf)- 1, ";colormask=0x%X", color.colorMask);
      break;
    case NO_COLOR:
      // do nothing
      break;
    }

    if ((dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_KEEP_CRC) == 0) {
      // Remove FCS
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";Slice=EndOfFrame[-4]");
    }

    // Set the tag name
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";tag=%s]=", internals->tagName);

    // Add the filter created info to the NTPL buffer
    strncpy(&ntpl_buf[strlen(ntpl_buf)], filter_buf1, NTPL_BSIZE - strlen(ntpl_buf) - 1);

    if (ntpl_str) {
      if (filterContinue) {
        snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " and");
      }
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " %s", ntpl_str);
      filterContinue = true;
    }

    if (filterContinue) {
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " and");
    }

    if (nb_ports == 0) {
    // Set the ports
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " port==%u", internals->port);
      filterContinue = true;
    }
    else {
      int ports;
      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, " port==");
      for (ports = 0; ports < nb_ports;ports++) {
        snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "%u", list_ports[ports]);
        if (ports < (nb_ports - 1)) {
          snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ",");
        }
      }
      filterContinue = true;
    }
  }
  
  if (CreateOptimizedFilterKeymatcher(ntpl_buf, internals, flow, &filterContinue, typeMask, list_queues, nb_queues, key, &color, error) != 0) {
    NTACC_UNLOCK(&internals->configlock);
    goto FlowError;
  }
  
  if (!reuse) {
    if (DoNtpl(ntpl_buf, &ntplID, internals, NULL) != 0) {
      goto FlowError;
    }

    NTACC_LOCK(&internals->lock);
    flow->assign_ntpl_id = ntplID;
    NTACC_UNLOCK(&internals->lock);
  }

  NTACC_UNLOCK(&internals->configlock);

  if (ntpl_buf) {
    rte_free(ntpl_buf);
  }
  if (filter_buf1) {
    rte_free(filter_buf1);
  }

  NTACC_LOCK(&internals->lock);
  LIST_INSERT_HEAD(&internals->flows, flow, next);
  NTACC_UNLOCK(&internals->lock);
  return flow;

FlowError:
    if (flow) {
      rte_free(flow);
    }
    if (ntpl_buf) {
      rte_free(ntpl_buf);
    }
    if (filter_buf1) {
      rte_free(filter_buf1);
    }
    return NULL;
}

/******************************************************
 Do only delete the assign command if it is not in used anymore.
 This means that is it not referenced in any other flow.
 No lock in this code
 *******************************************************/
static void _cleanUpAssignNtplId(uint32_t assignNtplID, struct pmd_internals *internals, struct rte_flow_error *error)
{
  char ntpl_buf[21];
  struct rte_flow *pFlow;
  LIST_FOREACH(pFlow, &internals->flows, next) {
    if (pFlow->assign_ntpl_id == assignNtplID) {
      // NTPL ID still in use
      return;
    }
  }
  // NTPL ID not in use
  PMD_NTACC_LOG(DEBUG, "Deleting assign filter: %u\n", assignNtplID);
  snprintf(ntpl_buf, 20, "delete=%d", assignNtplID);
  NTACC_LOCK(&internals->configlock);
  DoNtpl(ntpl_buf, NULL, internals, error);
  NTACC_UNLOCK(&internals->configlock);
}

/******************************************************
 Do only release the keyset if it is not in used anymore.
 This means that is it not referenced in any other flow.
 No lock in this code
 *******************************************************/
static void _cleanUpKeySet(int key, struct pmd_internals *internals, struct rte_flow_error *error)
{
  struct rte_flow *pTmp;
  LIST_FOREACH(pTmp, &internals->flows, next) {
    if (pTmp->key == key) {
      // Key set is still in use
      return;
    }
  }
  // Key set is not in use anymore. delete it.
  PMD_NTACC_LOG(DEBUG, "Returning keyset %u: %d\n", internals->adapterNo, key);
  DeleteKeyset(key, internals, error);
  ReturnKeysetValue(internals, key);
}

/******************************************************
 Delete a flow by deleting the NTPL command assigned
 with the flow. Check if some of the shared components
 like keyset and assign filter is still in use.
 No lock in this code
 *******************************************************/
static void _cleanUpFlow(struct rte_flow *flow, struct pmd_internals *internals, struct rte_flow_error *error)
{
  char ntpl_buf[21];
  PMD_NTACC_LOG(DEBUG, "Remove flow %p\n", flow);
  LIST_REMOVE(flow, next);
  while (!LIST_EMPTY(&flow->ntpl_id)) {
    struct filter_flow *id;
    id = LIST_FIRST(&flow->ntpl_id);
    snprintf(ntpl_buf, 20, "delete=%d", id->ntpl_id);
    NTACC_LOCK(&internals->configlock);
    DoNtpl(ntpl_buf, NULL, internals, NULL);
    NTACC_UNLOCK(&internals->configlock);
    PMD_NTACC_LOG(DEBUG, "Deleting Item filter: %s\n", ntpl_buf);
    LIST_REMOVE(id, next);
    rte_free(id);
  }
  _cleanUpAssignNtplId(flow->assign_ntpl_id, internals, error);
  _cleanUpKeySet(flow->key, internals, error);
  rte_free(flow);
}

int dev_flow_destroy_keymatcher(struct rte_eth_dev *dev,
                                struct rte_flow *flow,
                                struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  NTACC_LOCK(&internals->lock);
  _cleanUpFlow(flow, internals, error);
  NTACC_UNLOCK(&internals->lock);
  return 0;
}

int dev_flow_flush_keymatcher(struct rte_eth_dev *dev,
                              struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;

  NTACC_LOCK(&internals->lock);
  while (!LIST_EMPTY(&internals->flows)) {
    struct rte_flow *flow;
    flow = LIST_FIRST(&internals->flows);
    _cleanUpFlow(flow, internals, error);
  }
  NTACC_UNLOCK(&internals->lock);
  return 0;
}

