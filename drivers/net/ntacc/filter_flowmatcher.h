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

#ifndef __FILTER_FLOWMATCHER_H__
#define __FILTER_FLOWMATCHER_H__

int ReuseNtplID(uint8_t port, uint8_t priority, uint8_t keyID, uint8_t keySetID, uint32_t *ntplID);

int ReuseKeySetID(uint8_t *pList_queues, uint8_t nb_queues, uint8_t forwardPort, uint8_t *keySetID);

int ReuseKeyID(uint64_t typeMask, uint8_t *keyID);

int CreateOptimizedFilterFlowmatcher1(struct pmd_internals *internals,
                                      uint64_t typeMask,
                                      uint8_t keyID,
                                      struct color_s *pColor,
                                      struct rte_flow_error *error);

int CreateOptimizedFilterFlowmatcher2(struct pmd_internals *internals,
                                      struct rte_flow *flow,
                                      uint8_t keyID,
                                      uint8_t keySetID,
                                      uint64_t typeMask,
                                      struct color_s *pColor,
                                      struct rte_flow_error *error);

int UnlearnFlowFlowmatcher(struct pmd_internals *internals, struct rte_flow *flow);
void UpdateNtplIDFlowmatcher(uint32_t ntplID, uint8_t port, uint8_t priority, uint8_t keyID, uint8_t keySetID);
int GetKeySetID(uint8_t *pList_queues, uint8_t nb_queues, uint8_t forwardPort, uint8_t *keySetID);
int GetKeyID(uint64_t typeMask, uint8_t *keyID);
void DumpFlow(struct rte_flow *pFlow);
int ReleaseNtplID(struct pmd_internals *internals, struct rte_flow *flow);
int ReleaseKeySetID(struct pmd_internals *internals, struct rte_flow *flow);
int ReleaseKeyID(struct pmd_internals *internals, struct rte_flow *flow);

struct rte_flow {
	LIST_ENTRY(rte_flow) next;
  uint32_t ntplID;
  uint8_t  keyID;
  uint8_t  keySetID;
  uint8_t  ipProto;
  uint8_t  keyData[40];
};



#endif
