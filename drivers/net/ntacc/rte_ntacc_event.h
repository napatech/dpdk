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

#ifndef __RTE_NTACC_EVENT_H__
#define __RTE_NTACC_EVENT_H__

#define NTACC_EVENTDEV_NAME "event_ntacc"

// Must be set in the variable event in in the rte_event struct to select
// the flow information wanted.
enum eventFlowCommand_e { 
  GET_FLOW_DATA,
  GET_FLOW_STATUS
};

typedef struct eventFlowStatusData_s {
  uint32_t id;    /*< 32-bit user defined flow id */
  uint32_t flags; /*< Flags indicating the status of the operation */
} eventFlowStatusData_t;

typedef struct eventFlowData_s {
  uint64_t packets_a; /*< Packet counter for set A */
  uint64_t octets_a;  /*< Byte/octet counter for set A */
  uint64_t packets_b; /*< Packet counter for set B */
  uint64_t octets_b;  /*< Byte/octet counter for set B */
  uint64_t ts;        /*< Time stamp in UNIX_NS format of last seen packet */
  uint32_t id;        /*< 32-bit user defined ID */
  uint16_t flags_a;   /*< Bitwise OR of TCP flags for packets in set A */
  uint16_t flags_b;   /*< Bitwise OR of TCP flags for packets in set B */
} eventFlowData_t;

// Flow records are returned in the event_ptr in the rte_event struct.
// The Flow record struct is allocated by the driver and MUST be released by the 
// application. If event_ptr==NULL no records is returned due to an error.

// Flow status are returned in the variable u64 in the rte_event struct.
// Read the status like this ((eventFlowStatusData_t *)(&ev->u64))->flags and 
// ((eventFlowStatusData_t *)(&ev->u64))->id

// When using rte_event_dequeue_burst, The eventFlowCommand_e must be set on the first event packet
// by setting event in in the rte_event struct to either GET_FLOW_DATA or GET_FLOW_STATUS. The event 
// command are ignored in the following event packets


#endif /* __RTE_NTACC_EVENT_H__ */


