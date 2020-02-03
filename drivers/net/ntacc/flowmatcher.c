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
#include "flowmatcher.h"
#include "filter_flowmatcher.h"

struct rte_flow *dev_flow_flowmatcher_create(struct rte_eth_dev *dev,
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
  bool reuse = true;
  uint8_t keyID;
  uint8_t keySetID;
  int i;

  char *ntpl_buf = NULL;
  char *filter_buf1 = NULL;
  struct rte_flow *flow = NULL;
  const char *ntpl_str = NULL;

  // Init error struct
  rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_NONE, NULL, "No errors");

  NTACC_LOCK(&internals->configlock);

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
    goto FlowError;
  }

  if (attr->group) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL, "Attribute groups are not supported");
    goto FlowError;
  }
  if (attr->egress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL, "Attribute egress is not supported");
    goto FlowError;
  }
  if (!attr->ingress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, NULL, "Attribute ingress must be set");
    goto FlowError;
  }

  ReuseKeyID(typeMask, &keyID);
  ReuseKeySetID(list_queues, nb_queues, forwardPort, &keySetID);

  if (ReuseNtplID(internals->port, attr->priority, keyID, keySetID, &ntplID) != 0) {
    reuse = false;
    ntpl_buf = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
    if (!ntpl_buf) {
      rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
      goto FlowError;
    }

    if (action & (ACTION_RSS | ACTION_QUEUE)) {
      // This is not a Drop filter or a retransmit filter
      if (nb_queues == 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "A queue must be defined");
        goto FlowError;
      }

      // Check the queues
      for (i = 0; i < nb_queues; i++) {
        if (!internals->rxq[list_queues[i]].enabled) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "All defined queues must be enabled");
          goto FlowError;
        }
      }

      switch (color.type)
      {
      case NO_COLOR:
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=26,colorbits=14;", attr->priority);
        break;
      case ONE_COLOR:
        snprintf(ntpl_buf, NTPL_BSIZE, "assign[priority=%u;Descriptor=DYN3,length=22,colorbits=32;", attr->priority);
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

  if (typeMask != RX_FILTER) {
    //printf(">>>>>>>>>>>>>>>>>>>>>>>> 0 KeyID=%u - KeySetID=%u\n", keyID, keySetID);
    if (keyID == 0) {
      if (GetKeyID(typeMask, &keyID) != 0){
        rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Internal error: No available KeyID");
        goto FlowError;
      }
      if (CreateOptimizedFilterFlowmatcher1(internals, typeMask, keyID, &color, error) != 0) {
        goto FlowError;
      }
    }
    //printf(">>>>>>>>>>>>>>>>>>>>>>>> 1 KeyID=%u - KeySetID=%u\n", keyID, keySetID);
    if (keySetID == 0) {
      if (GetKeySetID(list_queues, nb_queues, forwardPort, &keySetID) != 0){
        rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Internal error: No available KeySetID");
        goto FlowError;
      }
    }

    if (!reuse) {
      if (filterContinue)
        strcat(ntpl_buf, " and ");

      snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, "key(kdflow%u,KeyID=%d,CounterSet=CSA)==%u", keyID, keyID, keySetID);
      if (DoNtpl(ntpl_buf, &ntplID, internals, NULL) != 0) {
        goto FlowError;
      }
      UpdateNtplIDFlowmatcher(ntplID, internals->port, attr->priority, keyID, keySetID);
    }

    //printf(">>>>>>>>>>>>>>>>>>>>>>>> 2 KeyID=%u - KeySetID=%u\n", keyID, keySetID);
    if (CreateOptimizedFilterFlowmatcher2(internals, flow, keyID, keySetID, typeMask, &color, error) != 0) {
      goto FlowError;
    }
  }
  else {
    if (DoNtpl(ntpl_buf, &ntplID, internals, NULL) != 0) {
      goto FlowError;
    }
    UpdateNtplIDFlowmatcher(ntplID, internals->port, attr->priority, 0, 0);
  }

  NTACC_UNLOCK(&internals->configlock);

  if (ntpl_buf) {
    rte_free(ntpl_buf);
  }
  if (filter_buf1) {
    rte_free(filter_buf1);
  }

  NTACC_LOCK(&internals->lock);
  flow->ntplID = ntplID;
  flow->keyID = keyID;
  flow->keySetID = keySetID;
  LIST_INSERT_HEAD(&internals->flows, flow, next);
  NTACC_UNLOCK(&internals->lock);
  DumpFlow(flow);
  return flow;

FlowError:
  NTACC_UNLOCK(&internals->lock);
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

static int _cleanFlowFlowmatcher(struct pmd_internals *internals, struct rte_flow *flow) 
{
  int error = 0;

  LIST_REMOVE(flow, next);
  // Unlearn flow
  if (flow->keyID && flow->keySetID) {
    DumpFlow(flow);
    if (UnlearnFlowFlowmatcher(internals, flow) != 0) {
      error = 1;
    }
    // Release NtplID
    if (ReleaseNtplID(internals, flow) != 0) {
      error = 1;
    }
    // Release KeySetID
    if (ReleaseKeySetID(internals, flow) != 0) {
      error = 1;
    }
    // Release Key ID
    if (ReleaseKeyID(internals, flow) != 0) {
      error = 1;
    }
  }
  else {
    // Release NtplID
    if (ReleaseNtplID(internals, flow) != 0) {
      error = 1;
    }
  }
  rte_free(flow);
  if (error) {
    return -1;
  }
  return 0;
}

int dev_flow_destroy_flowmatcher(struct rte_eth_dev *dev,
                                struct rte_flow *flow ,
                                struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  int retError;

  printf("Remove flow %p\n", flow);
  NTACC_LOCK(&internals->lock);
  retError = _cleanFlowFlowmatcher(internals, flow);
  NTACC_UNLOCK(&internals->lock);

  if (retError != 0) {
    rte_flow_error_set(error, EIO, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Failed to destroy flow correctly");
    return -1;
  }
  return 0;
}

int dev_flow_flush_flowmatcher(struct rte_eth_dev *dev,
                               struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  int retError = 0;

  NTACC_LOCK(&internals->lock);
  while (!LIST_EMPTY(&internals->flows)) {
    retError |= _cleanFlowFlowmatcher(internals, LIST_FIRST(&internals->flows));
  }
  NTACC_UNLOCK(&internals->lock);

  if (retError != 0) {
    rte_flow_error_set(error, EIO, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Failed to flush flows correctly");
    return -1;
  }
  return 0;
}

