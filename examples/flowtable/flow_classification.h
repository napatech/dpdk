/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Napatech. All rights reserved.
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
 *     * Neither the name of Napatech nor the names of its
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
#ifndef _FLOW_CLASSIFICATION_H_
#define _FLOW_CLASSIFICATION_H_

#include <rte_mbuf.h>

extern volatile int running;

#ifdef ENABLE_HW_OFFLOAD

#define RTE_FLOW_FILTER_SUPPORTED 1
#if 0
(rte_eth_dev_filter_supported(0, RTE_ETH_FILTER_GENERIC) == 0 && \
		 		                          rte_eth_dev_filter_supported(1, RTE_ETH_FILTER_GENERIC) == 0)
#endif
#else
#define RTE_FLOW_FILTER_SUPPORTED 0
#endif
/* Number of HW offload entries */
#define NUM_HW_OFFLOAD_ENTRIES (1 << 14)

/* Flow termination timeouts */
#define TIMEOUT_IDLE      5 /* When to timeout idle flows */
#define TIMEOUT_TCP_FIN   2 /* Timeout after TCP FIN before closing flows.
                               This is to catch out of order packets after
                               TCP_FIN terminate the flow */

int flow_classification_init(void **handle, unsigned int num_flows, int id_core);
int flow_classification_destroy(void *handle);
int flow_classification(void *handle, struct rte_mbuf **bufs, uint8_t nb_bufs,
	struct rte_mbuf **matched, uint8_t *nb_matched, uint64_t ts_ns);

#define LOG_INFO(...) do {\
	fprintf(stdout, __VA_ARGS__); \
} while(0)

#ifdef DEBUG
#define LOG_DEBUG(...) do {\
	fprintf(stdout, __VA_ARGS__); \
} while(0)
#else
#define LOG_DEBUG(...) do {} while(0);
#endif

#define LOG_ERROR(...) do {\
	fprintf(stdout, "%s() ln %d: ", __func__, __LINE__); \
	fprintf(stdout, __VA_ARGS__); \
} while(0)

#endif

