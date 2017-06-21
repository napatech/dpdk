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

#define _GNU_SOURCE

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stddef.h>

#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_table_hash.h>
#include <rte_malloc.h>
#include <rte_net.h>
#include <rte_flow.h>
#include <rte_tailq.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_lcore.h>
#include <rte_atomic.h>

#include "flow_classification.h"

#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PUSH 0x08
#define TCP_ACK  0x10
#define TCP_URC  0x20

// Ensure that the following is packed.
#pragma pack(push, 1)

/* Sorted tuple. ep[0] contain the lowest IP endpoint data */
struct flow_key {
  uint8_t ipv;           //!< IP version
  uint8_t ipproto;       //!< IP protocol
  struct {
		union {
		  uint8_t ipv6[16];      //!< IP address
			uint32_t ipv4;
		};
    uint16_t port;       //!< TCP/UDP port
  } ep[2];
	uint8_t spare[26];	//!< To ensure the structure is 64B (min next power2 size)
};

/* ep[0] contain the flow info where sip < dip and ep[1] other direction */
struct flow_meta {
  struct uni_dir_record {
		uint64_t lastTs;         //!< The last time a packet was seen
		uint64_t startTs;        //!< The time when the flow started
    uint64_t pkts;           //!< Packets received
    uint64_t bytes;          //!< Bytes received
    uint16_t tcpflags;       //!< TCP flags if applicable
  } ep[2];
	uint64_t lastSessionTs;
};

struct hw_offload_elem {
	enum {HW_OFFLOAD_ADD, HW_OFFLOAD_DEL} action;
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[3];
	struct rte_flow_action actions[3];
	union {
		struct rte_flow_item_tcp tcp;
		struct rte_flow_item_udp udp;
	} flow_l4;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} flow_l3;
	struct rte_flow_action_mark flow_action_mark;
	struct rte_flow_action_queue flow_action_queue;
#ifdef DEBUG
	uint32_t id;
#endif
	uint8_t port;
	struct rte_flow **rte_flow; // Initialized with the rte_flow when programmed
};

__extension__
typedef void    *MARKER[0];   /**< generic marker for a point in a structure */

enum flow_state {FLOW_STATE_ACTIVE, FLOW_STATE_RETIRE, FLOW_STATE_RETIRED};
struct flow_elem {
	struct flow_meta flow_meta;
	TAILQ_ENTRY(flow_elem) leTQ; //!< TailQ element.
	struct flow_key flow_key;
	enum flow_state flow_state;
	int added_to_hw; //!< Bitmak. Set if flow has been added to hw
	struct rte_flow *rte_flow[2]; //!< One for each port
	MARKER cacheline __rte_cache_min_aligned;
};

TAILQ_HEAD(FlowTQHead_s, flow_elem);
struct flow_db {
	struct rte_table_hash *table;	//!< Hash table
  struct rte_table_ops *ops;		//!< Table functions
	uint32_t *lookup_result[RTE_PORT_IN_BURST_SIZE_MAX];
	struct FlowTQHead_s lhActive;	//!< List of active flows sorted as LRU
	struct FlowTQHead_s lhRetire;	//!< FIFO of flows to be retired after TIMEOUT_TCP_FIN
	struct FlowTQHead_s lhRetired; //!< FIFO of flows that are retired
	struct flow_elem *flow_elem_array; //!< Array of flow elements
	uint32_t *index_stack;
	uint32_t index_stack_pos;

	/* HW offload */
	struct hw_offload_rings {
		struct rte_ring *enqueue;
		struct rte_ring *pool;
	} hw_offload_rings;
	void *hw_offload_elem_mem;
	int hw_offload_core_id;
	rte_atomic32_t hw_enqueue_num;

	/* Statistics */
	uint64_t num_total_flows;
	uint64_t num_current_flows;
	uint64_t num_unsupported_pkts;
	uint64_t num_dropped_flows;
	uint64_t num_packets_with_hw_offload[2];
	uint64_t num_packets_without_hw_offload[2];
	uint32_t num_current_hw_flows[2];
	uint32_t max_hw_flows[2];
	struct hw_offload_prog_tm {
		int64_t tns_tot;
		int32_t tns_cnt;
		int32_t tns_max;
		int32_t tns_min;
	} hw_offload_prog_tm[2]; /* 0 = Add, 1 = Del */
};

struct pkt_meta {
	struct rte_net_hdr_lens hdr_lens;
	uint32_t pkt_type;
	uint8_t ep;
	uint8_t tcpflags;
	uint64_t signature;
	struct flow_key key;
};

#define OFFSET_PKT_TYPE  sizeof(struct rte_mbuf)
#define OFFSET_SIGNATURE OFFSET_PKT_TYPE + offsetof(struct pkt_meta, signature)
#define OFFSET_KEY       OFFSET_PKT_TYPE + offsetof(struct pkt_meta, key)


// Disable 1-byte packing
#pragma pack(pop)

/* Convert a struct sockaddr address to a string, IPv4 and IPv6 */
static char *iptoa(uint8_t ipv, uint8_t *addr, char *s, uint maxlen)
{
  inet_ntop(ipv == 4 ? AF_INET : AF_INET6, addr, s, maxlen);
  return s;
}

#define MAX_STR_LEN 512
#define ADD_TO_STR(str, ...) do { snprintf(str + strlen(str), MAX_STR_LEN - strlen(str),  __VA_ARGS__); }while(0)

static void dump_tcp_flags(char *str, uint16_t tcpflags)
{
  if (tcpflags & TCP_FIN)  { ADD_TO_STR(str, "%s ", "FIN"); }
	if (tcpflags & TCP_SYN)  { ADD_TO_STR(str, "%s ", "SYN"); }
  if (tcpflags & TCP_RST)  { ADD_TO_STR(str, "%s ", "RST"); }
  if (tcpflags & TCP_PUSH) { ADD_TO_STR(str, "%s ", "PUSH");}
	if (tcpflags & TCP_ACK)  { ADD_TO_STR(str, "%s ", "ACK"); }
  if (tcpflags & TCP_URC)  { ADD_TO_STR(str, "%s ", "URC"); }
}

static void dump_netflow(struct flow_key *flow_key, struct flow_meta *flow_meta)
{
  char addr[32];
	char str[MAX_STR_LEN] = "\0";
	static int first = 1;
	if (first) {
		/* Add the header */
		ADD_TO_STR(str, "sIP\tdIP\tproto\tsPort\tdPort\tPackets\tBytes\tDuration (us)\ttcp flags\n");
		first = 0;
	}
	int i;
	for (i = 0; i < 2; i++) {
		if (flow_meta->ep[i].pkts) {
			ADD_TO_STR(str, "%s\t", iptoa(flow_key->ipv, flow_key->ep[i].ipv6, addr, (uint)sizeof(addr)));
			ADD_TO_STR(str, "%s\t",	iptoa(flow_key->ipv, flow_key->ep[i ^ 1].ipv6, addr, (uint)sizeof(addr)));
			ADD_TO_STR(str, "%s\t", flow_key->ipproto == 6 ? "TCP": flow_key->ipproto == 17 ? "UDP" : flow_key->ipproto == 132 ? "SCTP" : "ICMP");
			ADD_TO_STR(str, "%d\t%d\t", ntohs(flow_key->ep[i].port), ntohs(flow_key->ep[i ^ 1].port));
			ADD_TO_STR(str, "%ld\t%ld\t", flow_meta->ep[i].pkts, flow_meta->ep[i].bytes);
			ADD_TO_STR(str, "%ld\t", (flow_meta->ep[i].lastTs - flow_meta->ep[i].startTs)/1000);
			dump_tcp_flags(str, flow_meta->ep[i].tcpflags);
			ADD_TO_STR(str, "\n");
		}
	}
	fprintf(stderr, "%s", str);
}

/* Borrowed from hash_func.c in the pipeline example */
static inline uint64_t
hash_crc_key40(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0];
	k2 = k[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4]);

	crc0 = _mm_crc32_u64(crc0, crc1);
	crc1 = _mm_crc32_u64(crc2, crc3);

	crc0 ^= crc1;
	return crc0;
}


static inline int
extract_l4(struct pkt_meta *pkt_meta, void *l4, uint32_t pkt_type, uint8_t swapped)
{
	if (pkt_type & RTE_PTYPE_L4_UDP) {
		pkt_meta->key.ipproto = 17;
		const struct udp_hdr *udp_hdr = (const struct udp_hdr*)l4;
		if (! swapped) {
			pkt_meta->key.ep[0].port = udp_hdr->src_port;
			pkt_meta->key.ep[1].port = udp_hdr->dst_port;
		} else {
			pkt_meta->key.ep[0].port = udp_hdr->dst_port;
			pkt_meta->key.ep[1].port = udp_hdr->src_port;
		}
	} else if (pkt_type & RTE_PTYPE_L4_TCP) {
		pkt_meta->key.ipproto = 6;
		const struct tcp_hdr *tcp_hdr = (const struct tcp_hdr*)l4;
		if (! swapped) {
			pkt_meta->key.ep[0].port = tcp_hdr->src_port;
			pkt_meta->key.ep[1].port = tcp_hdr->dst_port;
		} else {
			pkt_meta->key.ep[0].port = tcp_hdr->dst_port;
			pkt_meta->key.ep[1].port = tcp_hdr->src_port;
		}
		pkt_meta->tcpflags = tcp_hdr->tcp_flags;
	} else if (pkt_type & RTE_PTYPE_L4_SCTP) {
		pkt_meta->key.ipproto = 132;
		const struct sctp_hdr *sctp_hdr = (const struct sctp_hdr*)l4;
		if (! swapped) {
			pkt_meta->key.ep[0].port = sctp_hdr->src_port;
			pkt_meta->key.ep[1].port = sctp_hdr->dst_port;
		} else {
			pkt_meta->key.ep[0].port = sctp_hdr->dst_port;
			pkt_meta->key.ep[1].port = sctp_hdr->src_port;
		}
	} else if (pkt_type & RTE_PTYPE_L4_ICMP) {
		pkt_meta->key.ipproto = pkt_meta->key.ipv == 4 ? 1: 58;
		/* We can use the IPv4 ICMP header because the first 3 fields are the same for ICMPv4 and ICMPv6 */
		const struct icmp_hdr *icmp_hdr = (const struct icmp_hdr*)l4;
		/*
		 * For ICMP flows, the Source Port is zero, and the Destination Port number
		 *  field codes ICMP message Type and Code (port = ICMP-Type * 256 + ICMP-Code).
		 */
		if (! swapped) {
				pkt_meta->key.ep[0].port = 0;
				pkt_meta->key.ep[1].port = (uint16_t)icmp_hdr->icmp_type << 8 | icmp_hdr->icmp_code;
		} else {
				pkt_meta->key.ep[0].port = (uint16_t)icmp_hdr->icmp_type << 8 | icmp_hdr->icmp_code;
				pkt_meta->key.ep[1].port = 0;
		}
	} else {
		return 1;
	}
	return 0;
}
static inline int
extract_ipv4_key(struct pkt_meta *pkt_meta, struct rte_mbuf *mb, uint32_t pkt_type, struct rte_net_hdr_lens *hdr_lens)
{
	uint8_t swapped = 0;

	const struct ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr *,hdr_lens->l2_len);
	pkt_meta->key.ipv = 4;
	if (ipv4_hdr->src_addr < ipv4_hdr->dst_addr) {
		pkt_meta->key.ep[0].ipv4 = ipv4_hdr->src_addr;
		pkt_meta->key.ep[1].ipv4 = ipv4_hdr->dst_addr;
	} else {
		swapped = 1;
		pkt_meta->key.ep[0].ipv4 = ipv4_hdr->dst_addr;
		pkt_meta->key.ep[1].ipv4 = ipv4_hdr->src_addr;
	}
	pkt_meta->ep = swapped;
	void *l4 = rte_pktmbuf_mtod_offset(mb, void *, hdr_lens->l2_len + hdr_lens->l3_len);
	return extract_l4(pkt_meta, l4, pkt_type, swapped);
}

static inline int
extract_ipv6_key(struct pkt_meta *pkt_meta, struct rte_mbuf *mb, uint32_t pkt_type, struct rte_net_hdr_lens *hdr_lens)
{
	struct ipv6_hdr *ipv6_hdr;
	uint8_t swapped = 0;
	ipv6_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv6_hdr *,hdr_lens->l2_len);
	pkt_meta->key.ipv = 6;
	if (memcmp(ipv6_hdr->src_addr, ipv6_hdr->dst_addr, sizeof(ipv6_hdr->src_addr)) < 0) {
		memcpy(pkt_meta->key.ep[0].ipv6, ipv6_hdr->src_addr, 16);
		memcpy(pkt_meta->key.ep[1].ipv6, ipv6_hdr->dst_addr, 16);
	} else {
		swapped = 1;
		memcpy(pkt_meta->key.ep[0].ipv6, ipv6_hdr->dst_addr, 16);
		memcpy(pkt_meta->key.ep[1].ipv6, ipv6_hdr->src_addr, 16);
	}
	pkt_meta->ep = swapped;
	void *l4 = rte_pktmbuf_mtod_offset(mb, void *, hdr_lens->l2_len + hdr_lens->l3_len);
	return extract_l4(pkt_meta, l4, pkt_type, swapped);
}


static inline int
prepare_metadata(struct rte_mbuf *mb)
{
	struct pkt_meta *pkt_meta = (struct pkt_meta*)RTE_MBUF_METADATA_UINT8_PTR(mb, OFFSET_PKT_TYPE);
	const uint32_t pkt_type = pkt_meta->pkt_type;
	struct rte_net_hdr_lens *hdr_lens = &pkt_meta->hdr_lens;

	/* Ensure the meta-data area is cleared */
	memset((uint8_t*)pkt_meta + offsetof(struct pkt_meta, ep),
					0,
					sizeof(struct pkt_meta) - offsetof(struct pkt_meta, ep));

	switch (pkt_type & (RTE_PTYPE_L3_MASK)) {
	case RTE_PTYPE_L3_IPV4:
		extract_ipv4_key(pkt_meta, mb, pkt_type, hdr_lens);
		break;
	case RTE_PTYPE_L3_IPV6:
		extract_ipv6_key(pkt_meta, mb, pkt_type, hdr_lens);
		break;
	default:
		// Unsupported packet
		pkt_meta->pkt_type = RTE_PTYPE_UNKNOWN;
		LOG_ERROR("Unsupported packet: PKT_TYPE: 0x%X\n", pkt_type);
		rte_pktmbuf_dump(stdout, mb, rte_pktmbuf_pkt_len(mb));
		return 1;
	}

	/* Calculate the signature */
	pkt_meta->signature = hash_crc_key40(&pkt_meta->key, 40, 0);

	LOG_DEBUG("IPv%d %s: %d.%d.%d.%d -> %d.%d.%d.%d  %d:%d\n",
		pkt_meta->key.ipv,
		pkt_meta->key.ipproto == 6 ? "TCP": "UDP",
		pkt_meta->key.ep[0].ipv4 & 0xFF,
		pkt_meta->key.ep[0].ipv4 >> 8 & 0xFF,
		pkt_meta->key.ep[0].ipv4 >> 16 & 0xFF,
		pkt_meta->key.ep[0].ipv4 >> 24 & 0xFF,
		pkt_meta->key.ep[1].ipv4 & 0xFF,
		pkt_meta->key.ep[1].ipv4 >> 8 & 0xFF,
		pkt_meta->key.ep[1].ipv4 >> 16 & 0xFF,
		pkt_meta->key.ep[1].ipv4 >> 24 & 0xFF,
		ntohs(pkt_meta->key.ep[0].port),
		ntohs(pkt_meta->key.ep[1].port));
	return 0;
}

#ifdef SHOW_FLOW_IN_HW_COUNT
volatile uint32_t flows_cur_in_hw = 0;
#endif
#ifdef ENABLE_HW_OFFLOAD
static void hw_offload_prepare_elem(struct hw_offload_elem *elem,
																		struct pkt_meta *pkt_meta,
																		uint8_t ep,
																		uint32_t id,
																		struct rte_flow **rte_flow,
																		uint8_t port)
{
	memset(elem, 0, sizeof(struct hw_offload_elem));
	elem->attr.ingress = 1;

	/* Prepare the pattern stack */
	if (pkt_meta->pkt_type & RTE_PTYPE_L3_IPV4) {
		elem->flow_l3.ipv4.hdr.src_addr = pkt_meta->key.ep[ep].ipv4;
		elem->flow_l3.ipv4.hdr.dst_addr = pkt_meta->key.ep[ep ^ 1].ipv4;
		elem->pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
		elem->pattern[0].spec = &elem->flow_l3.ipv4;
		elem->pattern[0].last = NULL;
		elem->pattern[0].mask = &rte_flow_item_ipv4_mask; //!< Declared in rte_flow.h
	} else {
		memcpy(elem->flow_l3.ipv6.hdr.src_addr, pkt_meta->key.ep[ep].ipv6, 16);
		memcpy(elem->flow_l3.ipv6.hdr.dst_addr, pkt_meta->key.ep[ep ^ 1].ipv6, 16);
		elem->pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV6;
		elem->pattern[0].spec = &elem->flow_l3.ipv6;
		elem->pattern[0].last = NULL;
		elem->pattern[0].mask = &rte_flow_item_ipv4_mask; //!< Declared in rte_flow.h
	}
	if (pkt_meta->pkt_type & RTE_PTYPE_L4_TCP) {
		elem->flow_l4.tcp.hdr.src_port = pkt_meta->key.ep[ep].port;
		elem->flow_l4.tcp.hdr.dst_port = pkt_meta->key.ep[ep ^ 1].port;
		elem->pattern[1].type = RTE_FLOW_ITEM_TYPE_TCP;
		elem->pattern[1].spec = &elem->flow_l4.tcp;
		elem->pattern[1].last = NULL;
		elem->pattern[1].mask = &rte_flow_item_tcp_mask; //!< Declared in rte_flow.h
	} else {
		elem->flow_l4.udp.hdr.src_port = pkt_meta->key.ep[ep].port;
		elem->flow_l4.udp.hdr.dst_port = pkt_meta->key.ep[ep ^ 1].port;
		elem->pattern[1].type = RTE_FLOW_ITEM_TYPE_UDP;
		elem->pattern[1].spec = &elem->flow_l4.udp;
		elem->pattern[1].last = NULL;
		elem->pattern[1].mask = &rte_flow_item_udp_mask; //!< Declared in rte_flow.h
	}
	elem->pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
	elem->pattern[2].spec = NULL;
	elem->pattern[2].last = NULL;
	elem->pattern[2].mask = NULL;

	/* Prepare the action stack */
	elem->flow_action_queue.index = 0;
	elem->actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	elem->actions[0].conf = &elem->flow_action_queue;
	elem->flow_action_mark.id = id;
	elem->actions[1].type = RTE_FLOW_ACTION_TYPE_MARK;
	elem->actions[1].conf = &elem->flow_action_mark;
	elem->actions[2].type = RTE_FLOW_ACTION_TYPE_END;
	elem->actions[2].conf = NULL;
	elem->rte_flow = rte_flow;
	elem->port = port;
	elem->action = HW_OFFLOAD_ADD;
	elem->rte_flow[0] = NULL;
	elem->rte_flow[1] = NULL;
#ifdef DEBUG
	elem->id = id;
#endif
}

static void flow_hw_enqueue(struct flow_db *flow_db, uint8_t port,
														struct pkt_meta *pkt_meta, int ep, uint32_t id,
														struct flow_elem *flow_elem)
{
	struct hw_offload_elem *hw_offload_elem;
	/* Pull an element from the pool */
	if (rte_ring_sc_dequeue(flow_db->hw_offload_rings.pool, (void**)&hw_offload_elem) != 0) {
		return;
	}
	/* Prepare the rte_flow stacks for this port */
	hw_offload_prepare_elem(hw_offload_elem, pkt_meta, ep, id, &flow_elem->rte_flow[port], port);
	/* Enqueue */
	if (rte_ring_sp_enqueue(flow_db->hw_offload_rings.enqueue, hw_offload_elem) != 0) {
		/* Return the pulled element to the pool */
		rte_ring_mp_enqueue(flow_db->hw_offload_rings.pool, hw_offload_elem);
		return;
	}
	/* Mark the element enqued */
	flow_elem->added_to_hw |= (1<<port);
}

static void flow_hw_add(struct flow_db *flow_db, struct flow_elem *flow_elem, struct rte_mbuf *mb)
{
	if (flow_db->hw_offload_core_id < 0) {
		/* HW offload not supported */
		return;
	}

	if ((rte_atomic32_read(&flow_db->hw_enqueue_num)+2) >= NUM_HW_OFFLOAD_ENTRIES) {
		return;
	}

	/*
	 * Check that we have at least NUM_HW_OFFLOAD_ENTRIES
	 * left in the enqueue ring otherwise we must bail out.
	 * +2 because we two entries and still have NUM_HW_OFFLOAD_ENTRIES left
	 * after the enqueue.
	 */
	if (rte_ring_free_count(flow_db->hw_offload_rings.enqueue) < (NUM_HW_OFFLOAD_ENTRIES + 2)) {
		return;
	}
	/* Do we have 2 free entries in the pool */
	if (rte_ring_count(flow_db->hw_offload_rings.pool) < 2) {
		return;
	}

	rte_atomic32_add(&flow_db->hw_enqueue_num, 2);

	struct pkt_meta *pkt_meta = (struct pkt_meta*)RTE_MBUF_METADATA_UINT8_PTR(mb, OFFSET_PKT_TYPE);
	assert(mb->port <= 1);

	uint32_t id = (uint32_t)(((uintptr_t)flow_elem -
															(uintptr_t)flow_db->flow_elem_array) /
															(uint32_t)sizeof(struct flow_elem));

	/* Make room for ep */
	id = (id << 1);

	LOG_DEBUG("Adding flow to HW\n");
	flow_hw_enqueue(flow_db, mb->port, pkt_meta, pkt_meta->ep, id | pkt_meta->ep, flow_elem);
	flow_hw_enqueue(flow_db, mb->port ^ 1, pkt_meta, pkt_meta->ep ^ 1, id | (pkt_meta->ep ^ 1), flow_elem);
}

static void flow_hw_remove(struct flow_db *flow_db, struct flow_elem *flow_elem)
{
	if (flow_db->hw_offload_core_id < 0) {
		/* HW offload not supported */
		return;
	}

	if(flow_elem->added_to_hw) {
		/* The flow has been added to HW. Enqueue a remove request */
		int port;
		LOG_DEBUG("Removing flow from HW\n");
		for (port = 0; port < 2; port++) {
			if (flow_elem->added_to_hw & (1 << port)) {
				struct hw_offload_elem *hw_offload_elem = NULL;
				/* Wait until have have a entry */
				while(running && (rte_ring_sc_dequeue(flow_db->hw_offload_rings.pool, (void**)&hw_offload_elem) != 0));
				if (running) {
#ifdef DEBUG
					hw_offload_elem->id = (uint32_t)(((uintptr_t)flow_elem - (uintptr_t)flow_db->flow_elem_array) / (uint32_t)sizeof(struct flow_elem));
#endif
					hw_offload_elem->action = HW_OFFLOAD_DEL;
					hw_offload_elem->port = port;
					hw_offload_elem->rte_flow = &flow_elem->rte_flow[port];
					flow_elem->added_to_hw &= ~(1 << port);
					/* Enqeueu the element */
					while(running && (rte_ring_sp_enqueue(flow_db->hw_offload_rings.enqueue, (void*)hw_offload_elem)!= 0));
				}
			}
		}
		rte_atomic32_sub(&flow_db->hw_enqueue_num, 2);
		assert(rte_atomic32_read(&flow_db->hw_enqueue_num) >=0);
	}
}

static int
hw_offload_scheduler(void *arg)
{
	struct flow_db *flow_db = (struct flow_db*)arg;

	while(running) {
		struct rte_flow_error rte_flow_error;
		struct hw_offload_elem *hw_offload_elem;
		/* Pull an element from the enqueue ring */
		if (rte_ring_sc_dequeue(flow_db->hw_offload_rings.enqueue, (void**)&hw_offload_elem) != 0) {
			usleep(10);
			continue;
		}

		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		const int64_t ts0 = (ts.tv_sec*1000000000+ts.tv_nsec);
		if (hw_offload_elem->action == HW_OFFLOAD_ADD) {
			LOG_DEBUG("Adding flow id %d port %d\n", hw_offload_elem->id, hw_offload_elem->port);
			/* Program the flow */
			struct rte_flow *rte_flow =
				rte_flow_create(hw_offload_elem->port, &hw_offload_elem->attr,
												hw_offload_elem->pattern, hw_offload_elem->actions,
												&rte_flow_error);
			if (rte_flow == NULL) {
				LOG_DEBUG("Programming port %d failed: %d %s\n",
					hw_offload_elem->port, rte_flow_error.type, rte_flow_error.message);
			} else {
#ifdef SHOW_FLOW_IN_HW_COUNT
				flows_cur_in_hw++;
#endif
				flow_db->num_current_hw_flows[hw_offload_elem->port]++;
				if (flow_db->num_current_hw_flows[hw_offload_elem->port] > flow_db->max_hw_flows[hw_offload_elem->port]) {
					flow_db->max_hw_flows[hw_offload_elem->port] = flow_db->num_current_hw_flows[hw_offload_elem->port];
				}
			}
			*hw_offload_elem->rte_flow = rte_flow;
		} else {
			LOG_DEBUG("Removing flow id %d port %d\n", hw_offload_elem->id, hw_offload_elem->port);
			struct rte_flow *rte_flow = *hw_offload_elem->rte_flow;
			if (rte_flow) {
				rte_flow_destroy(hw_offload_elem->port, rte_flow, NULL);
				*hw_offload_elem->rte_flow = NULL;
#ifdef SHOW_FLOW_IN_HW_COUNT
			flows_cur_in_hw--;
#endif
			} else {
				printf("Nothing to remove\n");
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &ts);
		const int64_t ts1 = (ts.tv_sec*1000000000+ts.tv_nsec);
		const int64_t tns = ts1 - ts0;
		struct hw_offload_prog_tm *hptm = &flow_db->hw_offload_prog_tm[hw_offload_elem->action];
		hptm->tns_tot += tns;
		hptm->tns_cnt++;
		hptm->tns_max = hptm->tns_max < tns ? tns : hptm->tns_max;
		hptm->tns_min = hptm->tns_min > tns ? tns : hptm->tns_min;

		/* Return the pulled element to the pool */
		rte_ring_mp_enqueue(flow_db->hw_offload_rings.pool, hw_offload_elem);
	}
	return 0;
}
#endif

static void flow_classification_clean_up(struct flow_db *flow_db)
{
	if (flow_db) {
		if(flow_db->hw_offload_core_id >=0) {
			rte_eal_wait_lcore(flow_db->hw_offload_core_id);
		}
		if (flow_db->flow_elem_array) {
			rte_free(flow_db->flow_elem_array);
		}
		if (flow_db->index_stack) {
			rte_free(flow_db->index_stack);
		}
		if (flow_db->table) {
			flow_db->ops->f_free(flow_db->table);
		}
		if (flow_db->hw_offload_rings.enqueue) {
			rte_ring_free(flow_db->hw_offload_rings.enqueue);
		}
		if (flow_db->hw_offload_rings.pool) {
			rte_ring_free(flow_db->hw_offload_rings.pool);
		}
		if (flow_db->hw_offload_elem_mem) {
			rte_free(flow_db->hw_offload_elem_mem);
		}
		rte_free(flow_db);
	}
}

static inline void
update_flow_stat(struct flow_db *flow_db, struct rte_mbuf *mb, uint32_t index, uint64_t ts_ns)
{
	struct pkt_meta* pkt_meta = (struct pkt_meta*)RTE_MBUF_METADATA_UINT8_PTR(mb, OFFSET_PKT_TYPE);
	LOG_DEBUG("Updating index %d\n", index);
	struct flow_elem *flow_elem = (struct flow_elem*)&flow_db->flow_elem_array[index];
	const uint8_t ep = pkt_meta->ep;
	struct uni_dir_record *record = (struct uni_dir_record*)&flow_elem->flow_meta.ep[ep];
	/* If this is a newly created flow capture the start time */
	if (record->pkts == 0) {
		record->startTs = ts_ns;
	}
	record->pkts++;
	record->bytes += rte_pktmbuf_pkt_len(mb) -  (pkt_meta->hdr_lens.l2_len);
	LOG_DEBUG("TCP FLAGS: %X\n", pkt_meta->tcpflags);
	record->tcpflags |= pkt_meta->tcpflags;
	record->lastTs = ts_ns;
	flow_elem->flow_meta.lastSessionTs = ts_ns;

	/* Remove the flow from its current position */
	switch(flow_elem->flow_state) {
		case FLOW_STATE_ACTIVE:
			TAILQ_REMOVE(&flow_db->lhActive, flow_elem, leTQ);
			break;
		case FLOW_STATE_RETIRE:
			TAILQ_REMOVE(&flow_db->lhRetire, flow_elem, leTQ);
			break;
		case FLOW_STATE_RETIRED:
			TAILQ_REMOVE(&flow_db->lhRetired, flow_elem, leTQ);
			break;
	}

	/* Check the TCP flags to see if the flow should retire */
	if (record->tcpflags & TCP_RST) {
		/* Place the element at the tail of the retired list */
		TAILQ_INSERT_TAIL(&flow_db->lhRetired, flow_elem, leTQ);
	} else if (record->tcpflags & TCP_FIN) {
		/* Place the element at the tail of the retire list */
		TAILQ_INSERT_TAIL(&flow_db->lhRetire, flow_elem, leTQ);
	} else {
		/* Place the element at the tail of the active list */
		TAILQ_INSERT_TAIL(&flow_db->lhActive, flow_elem, leTQ);
	}
}

/*
 * Return -1 for unsupported packet
 * Return 0 is packet flow is already learned and populate index arg
 * Return 1 for unlearned packet flow
 */
static inline int
get_packet_flow_index(struct flow_db *flow_db, struct rte_mbuf *mb, uint32_t *index)
{
	/* Decode the packet */
	struct pkt_meta* pkt_meta = (struct pkt_meta*)RTE_MBUF_METADATA_UINT8_PTR(mb, OFFSET_PKT_TYPE);
	pkt_meta->pkt_type = rte_net_get_ptype(mb, &pkt_meta->hdr_lens,
		RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK);
#ifdef ENABLE_HW_OFFLOAD
	/* Skip packets that already contain an index */
	if (mb->ol_flags & PKT_RX_FDIR_ID) {
		flow_db->num_packets_with_hw_offload[mb->port]++;
		*index = mb->hash.fdir.hi >> 1;
		pkt_meta->ep = mb->hash.fdir.hi & 1;
		LOG_DEBUG("Index %d (%d.%d) found by HW\n", mb->hash.fdir.hi, mb->hash.fdir.hi >> 1, mb->hash.fdir.hi & 1);
		/* If the packet is TCP we need the tcpflags */
		if (pkt_meta->pkt_type & RTE_PTYPE_L4_TCP) {
			const struct tcp_hdr *tcp_hdr =
				(const struct tcp_hdr*)rte_pktmbuf_mtod_offset(mb, void *,
					pkt_meta->hdr_lens.l2_len + pkt_meta->hdr_lens.l3_len);
			pkt_meta->tcpflags = tcp_hdr->tcp_flags;
		}
	} else
#endif
	{
		flow_db->num_packets_without_hw_offload[mb->port]++;
		LOG_DEBUG("Packet not found in HW\n");
		/* Prepare the meta-data region of all packets */
		if (prepare_metadata(mb) != 0) {
			flow_db->num_unsupported_pkts++;
			/* Unsupported packets are forwarded by default */
			return -1;
		}
		uint64_t hit_mask=0;
		LOG_DEBUG("Perform SW lookup\n");
		flow_db->ops->f_lookup(flow_db->table, &mb, (uint64_t)1, &hit_mask, (void**)flow_db->lookup_result);
		if (hit_mask) {
			*index = *flow_db->lookup_result[0];
			LOG_DEBUG("Lookup success. Index is: %d\n", *index);
		} else {
			return 1;
		}
	}
	return 0;
}

/*
 * Return -1 if flow table is exhausted and flow is dropped
 * Return 0 if flow is added and initialize the index arg
 */

static inline int
learn_packet_flow(struct flow_db *flow_db, struct rte_mbuf *mb, uint32_t *index)
{
	struct pkt_meta* pkt_meta = (struct pkt_meta*)RTE_MBUF_METADATA_UINT8_PTR(mb, OFFSET_PKT_TYPE);
	/* Pull an index */
	if (flow_db->index_stack_pos == 0) {
		LOG_DEBUG("Flow table exhausted. Dropping flow.\n");
		flow_db->num_dropped_flows++;
		rte_pktmbuf_free(mb);
		return -1;
	}

	/* Pull an index from the index stack */
	*index = flow_db->index_stack[flow_db->index_stack_pos--];
	uint32_t *p_index = NULL;
	int key_found;
	int status = flow_db->ops->f_add(flow_db->table, &pkt_meta->key, index, &key_found, (void**)&p_index);
	assert(key_found == 0);
	if (status != 0) {
		LOG_DEBUG("Failed to add new entry. Dropping flow.\n");
		flow_db->num_dropped_flows++;
		rte_pktmbuf_free(mb);
		return -1;
	}
	assert(*p_index == *index);

	/* Initialize the flow element */
	struct flow_elem *flow_elem = &flow_db->flow_elem_array[*index];
	memset(&flow_elem->flow_meta, 0, sizeof(struct flow_meta));
	memcpy(&flow_elem->flow_key, &pkt_meta->key, sizeof(struct flow_key));
	flow_elem->flow_state = FLOW_STATE_ACTIVE;

	/* Update statistics */
	flow_db->num_total_flows++;
	flow_db->num_current_flows++;
	/* Place the element at the tail of the lhActive */
	TAILQ_INSERT_TAIL(&flow_db->lhActive, flow_elem, leTQ);

#ifdef ENABLE_HW_OFFLOAD
	/* Place the entry in HW */
	flow_hw_add(flow_db, flow_elem, mb);
#endif
	LOG_DEBUG("Entry ADDED index %d\n", *p_index);
	return 0;
}

static inline void
unlearn_packet_flow(struct flow_db *flow_db, struct flow_elem *p)
{
	uint32_t index = (uint32_t)(((uintptr_t)p -
		(uintptr_t)flow_db->flow_elem_array) / (uint32_t)sizeof(struct flow_elem));
	int key_found;
	LOG_DEBUG("Remove index %d %p %p %ld %d!!!\n",
		index, p, flow_db->flow_elem_array, sizeof(struct flow_elem),
		(uint32_t)(p - flow_db->flow_elem_array));
	int status = flow_db->ops->f_delete(flow_db->table, &p->flow_key, &key_found, NULL);
	if (status != 0) {
		LOG_ERROR("Failed to delete table entry in SW\n");
	}
#ifdef ENABLE_HW_OFFLOAD
	/* Remove the entry in HW */
	flow_hw_remove(flow_db, p);
#endif
	dump_netflow(&p->flow_key, &p->flow_meta);

	/* Return the index to the index stack */
	flow_db->index_stack[++flow_db->index_stack_pos] = index;
	flow_db->num_current_flows--;
}

int
#ifdef ENABLE_HW_OFFLOAD
flow_classification_init(void **handle, unsigned int num_flows, int id_core)
#else
flow_classification_init(void **handle, unsigned int num_flows, int id_core __rte_unused)
#endif
{
  struct flow_db *flow_db;
	struct rte_table_hash_ext_params table_hash_params = {
		.key_size = 64, // sizeof(flow_key)
		.n_keys = num_flows,
		.n_buckets = num_flows / 2,
		.n_buckets_ext = num_flows / 2,
		.f_hash = hash_crc_key40, // 40B hash function. Even though the key_size if 64 only the first 36 contain valid data
		.seed = 0,
		.signature_offset = OFFSET_SIGNATURE,
		.key_offset = OFFSET_KEY,
	};

	flow_db = rte_zmalloc_socket(NULL, sizeof(struct flow_db),
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (flow_db == NULL) {
		LOG_ERROR("Failed to allocate flow db\n");
		goto error_handling;
	}
	flow_db->hw_offload_prog_tm[0].tns_min = 0x7FFFFFFF;
	flow_db->hw_offload_prog_tm[1].tns_min = 0x7FFFFFFF;
	flow_db->ops = &rte_table_hash_ext_ops;
	flow_db->hw_offload_core_id = -1;

	flow_db->flow_elem_array = rte_zmalloc_socket(NULL, sizeof(struct flow_elem) * num_flows,
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (flow_db->flow_elem_array == NULL) {
		LOG_ERROR("Failed to allocate flow pointer array\n");
		goto error_handling;
	}

	flow_db->index_stack = rte_malloc_socket(NULL, sizeof(uint32_t) * num_flows,
		RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (flow_db->index_stack == NULL) {
		LOG_ERROR("Failed to allocate index stack\n");
		goto error_handling;
 	}

	/* Initialize the index stack */
	uint32_t i;
	for (i = 0; i < num_flows; i++) {
		flow_db->index_stack[i] = ((num_flows - 1) - i);
	}
	flow_db->index_stack_pos = num_flows - 1;

	flow_db->table = flow_db->ops->f_create(&table_hash_params, rte_socket_id(), sizeof(uint32_t));
	if (flow_db->table == NULL) {
		LOG_ERROR("Failed to create flow table\n");
		goto error_handling;
	}

	TAILQ_INIT(&flow_db->lhActive);
	TAILQ_INIT(&flow_db->lhRetire);
	TAILQ_INIT(&flow_db->lhRetired);

	/* Is HW offload (rte_flow) supported */
	if(RTE_FLOW_FILTER_SUPPORTED) {
		/*
		* Allocate HW offload enqueue and dequeue rings.
		* Double the entries because we must have space for all DELETE filter
		* operations and size must be power 2
		*/
		flow_db->hw_offload_rings.enqueue = rte_ring_create("hw_off_enq",
				NUM_HW_OFFLOAD_ENTRIES << 1,
				rte_socket_id(),
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (flow_db->hw_offload_rings.enqueue == NULL) {
			LOG_ERROR("Cannot create ring \"%s\"\n", "hw_off_enq");
			goto error_handling;
		}
		flow_db->hw_offload_rings.pool = rte_ring_create("hw_off_pool",
				NUM_HW_OFFLOAD_ENTRIES << 1,
				rte_socket_id(),
				RING_F_SC_DEQ);
		if (flow_db->hw_offload_rings.pool == NULL) {
			LOG_ERROR("Cannot create ring \"%s\"\n", "hw_off_pool");
			goto error_handling;
		}
		/* Allocate pool elements */
		flow_db->hw_offload_elem_mem = rte_zmalloc_socket(NULL,
			sizeof(struct hw_offload_elem) * (NUM_HW_OFFLOAD_ENTRIES << 1),
			RTE_CACHE_LINE_SIZE, rte_socket_id());
		if (flow_db->hw_offload_elem_mem == NULL) {
			LOG_ERROR("Failed to allocate HW offload elements\n");
			goto error_handling;
		}
		/* Enqueue the elements into the pool */
		for (i = 0; i < (NUM_HW_OFFLOAD_ENTRIES << 1) - 1; i++) {
			if (rte_ring_sp_enqueue(flow_db->hw_offload_rings.pool,
				(struct hw_offload_elem*)flow_db->hw_offload_elem_mem+i) != 0) {
				LOG_ERROR("Failed to enqueue HW offload elem %d into pool\n", i);
				goto error_handling;
			}
		}

#ifdef ENABLE_HW_OFFLOAD
		rte_atomic32_init(&flow_db->hw_enqueue_num);
		/* Start the HW offload scheduler. Start looking from id_core */
		id_core = rte_get_next_lcore(id_core, 1, 1);
		flow_db->hw_offload_core_id = rte_eal_remote_launch(hw_offload_scheduler, (void*)flow_db, id_core);
		printf("Running the HW offload scheduler on core %d\n", id_core);
#endif
	}

	*handle = flow_db;
	return 0;

error_handling:
	flow_classification_clean_up(flow_db);
	return 1;
}

int
flow_classification_destroy(void *handle)
{
	struct flow_db *flow_db = (struct flow_db*)handle;
	if (flow_db) {
		LOG_INFO("Total amount of flows: %ld\n", flow_db->num_total_flows);
		LOG_INFO("Current flows when terminating: %ld\n", flow_db->num_current_flows);
		LOG_INFO("Amount of unsupported packets: %ld\n", flow_db->num_unsupported_pkts);
		LOG_INFO("Amount of dropped flows: %ld\n", flow_db->num_dropped_flows);
		LOG_INFO("Packets without HW offload: Port0 %ld, Port1 %ld\n",
			flow_db->num_packets_without_hw_offload[0], flow_db->num_packets_without_hw_offload[1]);
		if (flow_db->hw_offload_core_id >= 0) {
			LOG_INFO("Packets with HW offload: Port0 %ld, Port1 %ld\n",
				flow_db->num_packets_with_hw_offload[0], flow_db->num_packets_with_hw_offload[1]);
			LOG_INFO("Max num HW flows: Port0: %d, Port1: %d\n", flow_db->max_hw_flows[0], flow_db->max_hw_flows[1]);
			LOG_INFO("Still enqueued in HW: %d\n", rte_atomic32_read(&flow_db->hw_enqueue_num));
			int i;
			for (i = 0; i < 2; i++) {
				struct hw_offload_prog_tm *htm = &flow_db->hw_offload_prog_tm[i];
				LOG_INFO("RTE_FLOW %s average %ld ns. Min %d ns | Max %d ns\n",
					i == 0 ? "ADD" : "DEL",
					htm->tns_cnt ? (uint64_t)(htm->tns_tot / htm->tns_cnt) : 0,
					htm->tns_min != 0x7FFFFFFF ? htm->tns_min : 0,
					htm->tns_max);
			}
		}
		flow_classification_clean_up(flow_db);
	}
	return 0;
}

int flow_classification(void *handle, struct rte_mbuf **bufs, uint8_t nb_bufs, struct rte_mbuf **matched, uint8_t *nb_matched, uint64_t ts_ns)
{
  struct flow_db *flow_db = (struct flow_db*)handle;

	uint8_t i;
#ifdef DEBUG
	if (nb_bufs)	LOG_DEBUG("----- Start processing %d packets\n", nb_bufs);
#endif
	uint8_t matched_idx=0;
	for (i = 0; i < nb_bufs; i++) {
		uint32_t index;
		switch (get_packet_flow_index(flow_db, bufs[i], &index)) {
			case -1: /* Unsupported packet */
				matched[matched_idx++] = bufs[i]; /* Forward by default */
				break;
			case 0: /* Flow already learned */
				update_flow_stat(flow_db, bufs[i], index, ts_ns);
				matched[matched_idx++] = bufs[i];
				break;
	  	case 1: /* Unlearned flow */
				if (learn_packet_flow(flow_db, bufs[i], &index) == 0) {
					/* Flow learned */
					update_flow_stat(flow_db, bufs[i], index, ts_ns);
					matched[matched_idx++] = bufs[i];
				}
				break;
			default:
			rte_exit(1, "Unhandled case from get_packet_flow_index\n");
		}
#ifdef RXONLY
		rte_pktmbuf_free(bufs[i]);
#endif
	}
#ifdef RXONLY
	*nb_matched = 0;
#else
	*nb_matched = matched_idx;
#endif

#ifdef DEBUG
	if (nb_bufs) LOG_DEBUG("----- Done processing %d packets\n", nb_bufs);
#endif

	// Check if any flows are hitting the first retire state
	struct flow_elem *p, *pt;
	TAILQ_FOREACH_SAFE(p, &flow_db->lhActive, leTQ, pt) {
		if ((ts_ns - p->flow_meta.lastSessionTs) >= (TIMEOUT_IDLE*1E9)) {
			TAILQ_REMOVE(&flow_db->lhActive, p, leTQ);
			if (p->flow_key.ipproto == 6) {
				TAILQ_INSERT_TAIL(&flow_db->lhRetire, p, leTQ);
				LOG_DEBUG("Retired flow to stage 2\n");
			} else {
				TAILQ_INSERT_TAIL(&flow_db->lhRetired, p, leTQ);
				LOG_DEBUG("Retired flow\n");
			}
		} else {
			/* No need to look any further. */
			break;
		}
 	}
	// Check if any flows are hitting the 2nd retire state
	TAILQ_FOREACH_SAFE(p, &flow_db->lhRetire, leTQ, pt) {
		if (((ts_ns - p->flow_meta.lastSessionTs) >= ((TIMEOUT_IDLE + TIMEOUT_TCP_FIN) * 1E9))) {
				TAILQ_REMOVE(&flow_db->lhRetire, p, leTQ);
				TAILQ_INSERT_TAIL(&flow_db->lhRetired, p, leTQ);
				LOG_DEBUG("Retired flow from stage 2\n");
		} else {
			/* No need to look any further. */
			break;
		}
	}
	// Check if any flows are retired
	TAILQ_FOREACH_SAFE(p, &flow_db->lhRetired, leTQ, pt) {
		TAILQ_REMOVE(&flow_db->lhRetired, p, leTQ);
		unlearn_packet_flow(flow_db, p);
		LOG_DEBUG("Retired flow\n");
	}
	return 0;
}
