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

#include <nt.h>

#include "rte_eth_ntacc.h"
#include "ntacc_shared.h"

static const char *GetSorted(enum rte_eth_hash_function func)
{
  if (func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
    return "XOR=true";
  }
  else {
    return "XOR=false";
  }
}

/**
 * Create the stream ID part of the NTPL assign command.
 *
 * The stream ID is created form the number of queues as a range
 * or if only one queue as a single value
 */
void CreateStreamid(char *ntpl_buf, struct pmd_internals *internals, uint32_t nb_queues, uint8_t *list_queues)
{
  bool range = true;
  char buf[21];
  uint32_t i;

  if (nb_queues > 1) {
    // We need to check whether we can set it up as a range or list
    for (i = 0; i < nb_queues - 1; i++) {
      if (list_queues[i] != (list_queues[i + 1] - 1)) {
        // The queues are not contigous, so we need to use a list
        range = false;
        break;
      }
    }
  }
  else {
    range = false;
  }

  strcat(ntpl_buf, "streamid=");
  if (range) {
    snprintf(buf, 20, "(%u..%u)", internals->rxq[list_queues[0]].stream_id, internals->rxq[list_queues[nb_queues - 1]].stream_id);
    strcat(ntpl_buf, buf);
  }
  else {
    for (i = 0; i < nb_queues; i++) {
      snprintf(buf, 20, "%u", internals->rxq[list_queues[i]].stream_id);
      strcat(ntpl_buf, buf);
      if (i < nb_queues - 1) {
        strcat(ntpl_buf, ",");
      }
    }
  }
}

void CreateHash(char *ntpl_buf, const struct rte_flow_action_rss *rss, struct pmd_internals *internals)
{
  enum rte_eth_hash_function func;
  bool tunnel = false;

  // Select either sorted or non-sorted hash
  switch (rss->func)
  {
  case RTE_ETH_HASH_FUNCTION_DEFAULT:
    if (internals->symHashMode == SYM_HASH_ENA_PER_PORT)
      func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
    else
      func = RTE_ETH_HASH_FUNCTION_DEFAULT;
    break;
  case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
    func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
    break;
  default:
    return;
  }

  // Select either inner tunnel or outer tunnel hash
  switch (rss->level)
  {
  case 0:
  case 1:
    tunnel = false;
    break;
  case 2:
    tunnel = true;
    break;
  }

  if (rss->types & ETH_RSS_NONFRAG_IPV4_OTHER) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[12]/32,HashWord4_7=%s[16]/32,HashWordP=%s,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel),
             GetLayer(PROTO, tunnel), GetSorted(func));
    return;
  }

  if ((rss->types & ETH_RSS_NONFRAG_IPV4_TCP) || (rss->types & ETH_RSS_NONFRAG_IPV4_UDP) || (rss->types & ETH_RSS_NONFRAG_IPV4_SCTP)) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[12]/32,HashWord4_7=%s[16]/32,HashWord8=%s[0]/32,HashWordP=%s,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel), GetLayer(LAYER4, tunnel),
             GetLayer(PROTO, tunnel), GetSorted(func));
    return;
  }

  if ((rss->types & ETH_RSS_IPV4) || (rss->types & ETH_RSS_FRAG_IPV4)) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[12]/32,HashWord4_7=%s[16]/32,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel), GetSorted(func));
    return;
  }

  if (rss->types & ETH_RSS_NONFRAG_IPV6_OTHER) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[8]/128,HashWord4_7=%s[24]/128,HashWordP=%s,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel),
             GetLayer(PROTO, tunnel), GetSorted(func));
    return;
  }

  if ((rss->types & ETH_RSS_NONFRAG_IPV6_TCP) ||
      (rss->types & ETH_RSS_IPV6_TCP_EX)      ||
      (rss->types & ETH_RSS_NONFRAG_IPV6_UDP) ||
      (rss->types & ETH_RSS_IPV6_UDP_EX)      ||
      (rss->types & ETH_RSS_NONFRAG_IPV6_SCTP)) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[8]/128,HashWord4_7=%s[24]/128,HashWord8=%s[0]/32,HashWordP=%s,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel), GetLayer(LAYER4, tunnel),
             GetLayer(PROTO, tunnel), GetSorted(func));
    return;
  }

  if ((rss->types & ETH_RSS_IPV6) || (rss->types & ETH_RSS_FRAG_IPV6) || (rss->types & ETH_RSS_IPV6_EX)) {
    snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1,
             ";Hash=HashWord0_3=%s[8]/128,HashWord4_7=%s[24]/128,%s",
             GetLayer(LAYER3, tunnel), GetLayer(LAYER3, tunnel), GetSorted(func));
    return;
  }

  snprintf(&ntpl_buf[strlen(ntpl_buf)], NTPL_BSIZE - strlen(ntpl_buf) - 1, ";Hash=roundrobin");
}

/**
 * Get the layer NTPL keyword. Either an outer or an inner
 * version.
 */
const char *GetLayer(enum layer_e layer, bool tunnel)
{
  switch (layer) {
  case LAYER2:
    if (tunnel)
      return "InnerLayer2Header";
    else
      return "Layer2Header";
  case LAYER3:
  case IP:
    if (tunnel)
      return "InnerLayer3Header";
    else
      return "Layer3Header";
  case LAYER4:
    if (tunnel)
      return "InnerLayer4Header";
    else
      return "Layer4Header";
  case VLAN:
    if (tunnel)
      return "InnerFirstVLAN";
    else
      return "FirstVLAN";
  case MPLS:
    if (tunnel)
      return "InnerFirstMPLS";
    else
      return "FirstMPLS";
  case PROTO:
    if (tunnel)
      return "InnerIpProtocol";
    else
      return "IpProtocol";
  }
  return "UNKNOWN";
}

