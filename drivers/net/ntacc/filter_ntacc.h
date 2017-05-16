/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef __FILTER_NTACC_H__
#define __FILTER_NTACC_H__

bool CheckFeatureLevel(uint32_t level);

int CreateHash(char *ntpl_buf, uint64_t rss_hf, struct pmd_internals *internals, struct rte_flow *flow, int priority);
void CreateStreamid(char *ntpl_buf, struct pmd_internals *internals, uint32_t nb_queues, uint8_t *list_queues);
int ReturnKeysetValue(uint8_t adapterNo, int value);
void pushNtplID(struct rte_flow *flow, uint32_t ntplId, uint64_t rss_hf);

int SetEthernetFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnel);
int SetIPV4Filter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnel);
int SetIPV6Filter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnel);
int SetUDPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnnel);
int SetSCTPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnnel);
int SetTCPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tunnnel);
int SetICMPFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl);
int SetVlanFilter(char *ntpl_buf, bool *fc, const struct rte_flow_item *item, bool tnl);

int SetTunnelFilter(char *ntpl_buf, bool *fc, int version, int type);

int CreateOptimizedFilter(char *ntpl_buf, struct pmd_internals *internals, struct rte_flow *flow, bool *fc);

enum {
  GTP_TUNNEL_TYPE,
  GRE_TUNNEL_TYPE,
  VXLAN_TUNNEL_TYPE,
  NVGRE_TUNNEL_TYPE,
  IP_IN_IP_TUNNEL_TYPE,
};

struct filter_values_s {
	LIST_ENTRY(filter_values_s) next;
  int size; 
  uint64_t mask;
  int offset;
  const char *layerString;
  uint32_t layer;
  union {
    struct {
      uint16_t specVal;
      uint16_t maskVal;
      uint16_t lastVal;
    } v16;
    struct {
      uint32_t specVal;
      uint32_t maskVal;
      uint32_t lastVal;
    } v32;
    struct {
      uint64_t specVal;
      uint64_t maskVal;
      uint64_t lastVal;
    } v64;
    struct {
      uint8_t specVal[16];
      uint8_t maskVal[16];
      uint8_t lastVal[16];
    } v128;
  } value;
};

#endif

