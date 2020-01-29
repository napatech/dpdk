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

#ifndef __FILTER_KEYMATCHER_H__
#define __FILTER_KEYMATCHER_H__

struct rte_flow {
	LIST_ENTRY(rte_flow) next;
  LIST_HEAD(_filter_flows, filter_flow) ntpl_id;
  uint32_t assign_ntpl_id;
  uint8_t port;
  uint8_t forwardPort;
  uint8_t  key;
  uint64_t typeMask;
  uint64_t rss_hf;
  int priority;
  uint8_t nb_queues;
  uint8_t list_queues[NUMBER_OF_QUEUES];
};

/******************* Macros ********************/

#define NON_ZERO2(a)  (*a != 0 || *(a + 1) != 0)
#define NON_ZERO4(a)  (*a != 0 || *(a + 1) != 0 || *(a + 2) != 0 || *(a + 3) != 0)
#define NON_ZERO6(a)  (a[0] != 0  || a[1] != 0  || a[2] != 0  || a[3] != 0 || a[4] != 0  || a[5] != 0)
#define NON_ZERO16(a) (a[0] != 0  || a[1] != 0  || a[2] != 0  || a[3] != 0 ||  \
                       a[4] != 0  || a[5] != 0  || a[6] != 0  || a[7] != 0 ||  \
                       a[8] != 0  || a[9] != 0  || a[10] != 0 || a[11] != 0 || \
                       a[12] != 0 || a[13] != 0 || a[14] != 0 || a[15] != 0)

#define IPV4_ADDRESS(a) ((const char *)&a)[3] & 0xFF, ((const char *)&a)[2] & 0xFF, \
                        ((const char *)&a)[1] & 0xFF, ((const char *)&a)[0] & 0xFF

#define IPV6_ADDRESS(a) a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF,    \
                        a[4] & 0xFF, a[5] & 0xFF, a[6] & 0xFF, a[7] & 0xFF,    \
                        a[8] & 0xFF, a[9] & 0xFF, a[10] & 0xFF, a[11] & 0xFF,  \
                        a[12] & 0xFF, a[13] & 0xFF, a[14] & 0xFF, a[15] & 0xFF

#define MAC_ADDRESS2(a) a[5] & 0xFF, a[4] & 0xFF, a[3] & 0xFF, a[2] & 0xFF, a[1] & 0xFF, a[0] & 0xFF,    \
                        a[11] & 0xFF, a[10] & 0xFF, a[9] & 0xFF, a[8] & 0xFF, a[7] & 0xFF, a[6] & 0xFF,  \
                        a[12] & 0xFF, a[13] & 0xFF, a[14] & 0xFF, a[15] & 0xFF

#define MAC_ADDRESS(a)  a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF, a[4] & 0xFF, a[5] & 0xFF

#define MAC_ADDRESS_SWAP(a,b)  {b[5]=a[0];b[4]=a[1];b[3]=a[2];b[2]=a[3];b[1]=a[4];b[0]=a[5];}

#if 0
#define PRINT_IPV4(a, b) { uint32_t c = b; printf("%s: %d.%d.%d.%d\n", a, IPV4_ADDRESS(c)); }
#else
#define PRINT_IPV4(a, b)
#endif

#define CHECK8(a, b)   (a != NULL && (a->b != 0 && a->b != 0xFF))
#define CHECK16(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFF))
#define CHECK32(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFFFFFF))
#define CHECK64(a, b)  (a != NULL && (a->b != 0 && a->b != 0xFFFFFFFFFFFFFFFF))
#define CHECKIPV6(a)   _CheckArray(a, 16)
#define CHECKETHER(a)  _CheckArray(a, 6)

/******************* Function Prototypes ********************/

int CreateHashModeHash(uint64_t rss_hf, struct pmd_internals *internals, struct rte_flow *flow, int priority);
int ReturnKeysetValue(struct pmd_internals *internals, int value);
void pushNtplID(struct rte_flow *flow, uint32_t ntplId);

int SetEthernetFilter(const struct rte_flow_item *item,
                      bool tunnel,
                      uint64_t *typeMask,
                      struct pmd_internals *internals,
                      struct rte_flow_error *error);
int SetIPV4Filter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnel, uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetIPV6Filter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnel,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetUDPFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 bool tunnnel,
                 uint64_t *typeMask,
                 struct pmd_internals *internals,
                 struct rte_flow_error *error);
int SetSCTPFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tunnnel,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetTCPFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 bool tunnnel,
                 uint64_t *typeMask,
                 struct pmd_internals *internals,
                 struct rte_flow_error *error);
int SetICMPFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetVlanFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetMplsFilter(char *ntpl_buf,
                  bool *fc,
                  const struct rte_flow_item *item,
                  bool tnl,
                  uint64_t *typeMask,
                  struct pmd_internals *internals,
                  struct rte_flow_error *error);
int SetGreFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 uint64_t *typeMask);
int SetGtpFilter(char *ntpl_buf,
                 bool *fc,
                 const struct rte_flow_item *item,
                 uint64_t *typeMask,
                 int protocol);
int SetTunnelFilter(char *ntpl_buf,
                    bool *fc,
                    int type,
                    uint64_t *typeMask);

int CreateOptimizedFilterKeymatcher(char *ntpl_buf,
                                    struct pmd_internals *internals,
                                    struct rte_flow *flow,
                                    bool *fc,
                                    uint64_t typeMask,
                                    uint8_t *plist_queues,
                                    uint8_t nb_queues,
                                    int key,
                                    struct color_s *pColor,
                                    struct rte_flow_error *error);

void DeleteKeyset(int key, struct pmd_internals *internals, struct rte_flow_error *error);
//void DeleteHash(uint64_t rss_hf, uint8_t port, int priority, struct pmd_internals *internals);
void FlushHash(struct pmd_internals *internals);
bool IsFilterReuseKeymatcher(struct pmd_internals *internals, uint64_t typeMask, uint8_t *plist_queues, uint8_t nb_queues, int *key);

#endif

