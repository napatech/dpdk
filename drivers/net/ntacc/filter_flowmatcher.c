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

//#define NUMBER_OF_KEY_IDS 256
//#define NUMBER_OF_KEY_SET_IDS 15
//#define NUMBER_OF_NTPL_IDS 256

#define NUMBER_OF_KEY_IDS 10
#define NUMBER_OF_KEY_SET_IDS 15
#define NUMBER_OF_NTPL_IDS 10

struct _keyID_s {
  uint32_t refCount;
  uint64_t typeMask;
  uint32_t ntplID1;
  uint32_t ntplID2;
} sharedKeyID[NUMBER_OF_KEY_IDS];

struct _keySetID_s {
  uint32_t refCount;
  uint8_t list_queues[NUMBER_OF_QUEUES];
  uint8_t nb_queues;
  uint8_t forwardPort;
} sharedKeySetID[NUMBER_OF_KEY_SET_IDS];

struct _NTPL_ID_s {
  uint32_t refCount;
  uint32_t ntplID;
  uint8_t keyID;
  uint8_t keySetID;
  uint8_t port;
  uint8_t priority;
} sharedNtplID[NUMBER_OF_NTPL_IDS];

void initFlowmatcher(void)
{
  memset(sharedKeyID, 0, sizeof(sharedKeyID));
  memset(sharedKeySetID, 0, sizeof(sharedKeySetID));
  memset(sharedNtplID, 0, sizeof(sharedNtplID));
}

#define IPV6_ADDRESS(a) a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF,    \
                        a[4] & 0xFF, a[5] & 0xFF, a[6] & 0xFF, a[7] & 0xFF,    \
                        a[8] & 0xFF, a[9] & 0xFF, a[10] & 0xFF, a[11] & 0xFF,  \
                        a[12] & 0xFF, a[13] & 0xFF, a[14] & 0xFF, a[15] & 0xFF

static void dumpMask(uint64_t mask)
{
  printf("===============================================\n");
  if (mask & RX_FILTER) {
    printf("RX_FILTER ");
  }
  if (mask & DROP_FILTER) {
    printf("DROP_FILTER ");
  }
  if (mask & RETRANSMIT_FILTER) {
    printf("RETRANSMIT_FILTER ");
  }
  if (mask & DUMMY_FILTER) {
    printf("DUMMY_FILTER ");
  }
  if (mask & ETHER_ADDR_DST) {
    printf("ETHER_ADDR_DST ");
  }
  if (mask & ETHER_ADDR_SRC) {
    printf("ETHER_ADDR_SRC ");
  }
  if (mask & ETHER_TYPE) {
    printf("ETHER_TYPE ");
  }
  if (mask & IPV4_VERSION_IHL) {
    printf("IPV4_VERSION_IHL ");
  }
  if (mask & IPV4_TYPE_OF_SERVICE) {
    printf("IPV4_TYPE_OF_SERVICE ");
  }
  if (mask & IPV4_TOTAL_LENGTH) {
    printf("IPV4_TOTAL_LENGTH ");
  }
  if (mask & IPV4_PACKET_ID) {
    printf("IPV4_PACKET_ID ");
  }
  if (mask & IPV4_FRAGMENT_OFFSET) {
    printf("IPV4_FRAGMENT_OFFSET ");
  }
  if (mask & IPV4_TIME_TO_LIVE) {
    printf("IPV4_TIME_TO_LIVE ");
  }
  if (mask & IPV4_NEXT_PROTO_ID) {
    printf("IPV4_NEXT_PROTO_ID ");
  }
  if (mask & IPV4_HDR_CHECKSUM) {
    printf("IPV4_HDR_CHECKSUM ");
  }
  if (mask & IPV4_SRC_ADDR) {
    printf("IPV4_SRC_ADDR ");
  }
  if (mask & IPV4_DST_ADDR) {
    printf("IPV4_DST_ADDR ");
  }
  if (mask & IPV6_VTC_FLOW) {
    printf("IPV6_VTC_FLOW ");
  }
  if (mask & IPV6_PAYLOAD_LEN) {
    printf("IPV6_PAYLOAD_LEN ");
  }
  if (mask & IPV6_PROTO) {
    printf("IPV6_PROTO ");
  }
  if (mask & IPV6_HOP_LIMITS) {
    printf("IPV6_HOP_LIMITS ");
  }
  if (mask & IPV6_SRC_ADDR) {
    printf("IPV6_SRC_ADDR ");
  }
  if (mask & IPV6_DST_ADDR) {
    printf("IPV6_DST_ADDR ");
  }
  if (mask & TCP_SRC_PORT) {
    printf("TCP_SRC_PORT ");
  }
  if (mask & TCP_DST_PORT) {
    printf("TCP_DST_PORT ");
  }
  if (mask & TCP_SENT_SEQ) {
    printf("TCP_SENT_SEQ ");
  }
  if (mask & TCP_RECV_ACK) {
    printf("TCP_RECV_ACK ");
  }
  if (mask & TCP_DATA_OFF) {
    printf("TCP_DATA_OFF ");
  }
  if (mask & TCP_FLAGS) {
    printf("TCP_FLAGS ");
  }
  if (mask & TCP_RX_WIN) {
    printf("TCP_RX_WIN ");
  }
  if (mask & TCP_CKSUM) {
    printf("TCP_CKSUM ");
  }
  if (mask & TCP_URP) {
    printf("TCP_URP ");
  }
  if (mask & UDP_SRC_PORT) {
    printf("UDP_SRC_PORT ");
  }
  if (mask & UDP_DST_PORT) {
    printf("UDP_DST_PORT ");
  }
  if (mask & UDP_DGRAM_LEN) {
    printf("UDP_DGRAM_LEN ");
  }
  if (mask & UDP_DGRAM_CKSUM) {
    printf("UDP_DGRAM_CKSUM ");
  }
  if (mask & SCTP_SRC_PORT) {
    printf("SCTP_SRC_PORT ");
  }
  if (mask & SCTP_DST_PORT) {
    printf("SCTP_DST_PORT ");
  }
  if (mask & SCTP_TAG) {
    printf("SCTP_TAG ");
  }
  if (mask & SCTP_CKSUM) {
    printf("SCTP_CKSUM ");
  }
  if (mask & ICMP_TYPE) {
    printf("ICMP_TYPE ");
  }
  if (mask & ICMP_CODE) {
    printf("ICMP_CODE ");
  }
  if (mask & ICMP_CKSUM) {
    printf("ICMP_CKSUM ");
  }
  if (mask & ICMP_IDENT) {
    printf("ICMP_IDENT ");
  }
  if (mask & ICMP_SEQ_NB) {
    printf("ICMP_SEQ_NB ");
  }
  if (mask & VLAN_TCI) {
    printf("VLAN_TCI ");
  }
  if (mask & GTPU0_TUNNEL) {
    printf("GTPU0_TUNNEL ");
  }
  if (mask & GTPU1_TUNNEL) {
    printf("GTPU1_TUNNEL ");
  }
  if (mask & GREV0_TUNNEL) {
    printf("GREV0_TUNNEL ");
  }
  if (mask & VXLAN_TUNNEL) {
    printf("VXLAN_TUNNEL ");
  }
  if (mask & NVGRE_TUNNEL) {
    printf("NVGRE_TUNNEL ");
  }
  if (mask & IP_IN_IP_TUNNEL) {
    printf("IP_IN_IP_TUNNEL ");
  }
  if (mask & GTPC2_TUNNEL) {
    printf("GTPC2_TUNNEL ");
  }
  if (mask & GTPC1_TUNNEL) {
    printf("GTPC1_TUNNEL ");
  }
  if (mask & GTPC1_2_TUNNEL) {
    printf("GTPC1_2_TUNNEL ");
  }
  if (mask & GREV1_TUNNEL) {
    printf("GREV1_TUNNEL ");
  }
  if (mask & MPLS_LABEL) {
    printf("MPLS_LABEL ");
  }
  printf("\n");
}

static int DeleteNtplID(struct pmd_internals *internals, uint32_t ntplID)
{
  char buf[20];
  snprintf(buf, 20, "delete=%u", ntplID);
  if (DoNtpl(buf, NULL, internals, NULL)) {
    return -1;
  }
  return 0;
}

/**
 * Get a KeyID value from the KeyID pool. Used by the 
 * flowmatcher command 
 */
int GetKeyID(uint64_t typeMask, uint8_t *keyID)
{
  int i;

  // We need a new key ID
  for (i = 1; i < NUMBER_OF_KEY_IDS; i++) {
    if (sharedKeyID[i].refCount == 0) {
      sharedKeyID[i].refCount = 1;
      sharedKeyID[i].typeMask = typeMask;
      printf("New keyID %u\n", i);
      *keyID = i;
      return 0;
    }
  }
  *keyID = 0;
  return -1;
}

int ReuseKeyID(uint64_t typeMask, uint8_t *keyID)
{
  int i;

  // Do we have an active key ID already?
  dumpMask(typeMask);
  for (i = 1; i < NUMBER_OF_KEY_IDS; i++) {
    //printf("ReuseKeyID: refCount %u typeMask %08llX = %08llX\n", sharedKeyID[i].refCount, (unsigned long long int)sharedKeyID[i].typeMask, (unsigned long long int)typeMask);
    if (sharedKeyID[i].refCount > 0 && sharedKeyID[i].typeMask == typeMask) {
      sharedKeyID[i].refCount++;
      printf("Reuse keyID %u\n", i);
      *keyID = i;
      return 0;
    }
  }
  printf("No keyID for reuse\n");
  *keyID = 0;
  return -1;
}

static void UpdateKeyIDNtplID(uint8_t keyID, uint32_t ntplID1, uint32_t ntplID2)
{
  if (sharedKeyID[keyID].refCount == 0) {
    printf("===================================> Update keyID - Somethings gone wrong - refCount is zero\n");
  }

  if (sharedKeyID[keyID].ntplID1 != 0 || sharedKeyID[keyID].ntplID2 != 0) {
    printf("===================================> Update keyID - Somethings gone wrong - NTPL ID is not zero\n");
  }
  sharedKeyID[keyID].ntplID1 = ntplID1;
  sharedKeyID[keyID].ntplID2 = ntplID2;
}


/**
 * Release a KeyID value to the KeyID pool. Used by the 
 * flowmatcher command 
 */
int ReleaseKeyID(struct pmd_internals *internals, struct rte_flow *flow)
{
  if (flow->keyID >= NUMBER_OF_KEY_IDS) {
    printf("Failed destroying flow - KeyID %u illegal\n", flow->keyID);
    return -1;
  }
  
  if (sharedKeyID[flow->keyID].refCount == 0) {
    printf("Failed destroying flow - KeyID %u not in use\n", flow->keyID);
    return -1;
  }

  sharedKeyID[flow->keyID].refCount--;
  if (sharedKeyID[flow->keyID].refCount == 0) {
    printf("ReleaseKeyID: KeyID %u released\n", flow->keyID);

    // NTPL no longer needed. Delete them
    printf("ReleaseKeyID: Deleting NTPL2 %u\n", sharedKeyID[flow->keyID].ntplID2);
    if (DeleteNtplID(internals, sharedKeyID[flow->keyID].ntplID2) != 0)
      return -1;

    printf("ReleaseKeyID: Deleting NTPL1 %u\n", sharedKeyID[flow->keyID].ntplID1);
    if (DeleteNtplID(internals, sharedKeyID[flow->keyID].ntplID1) != 0)
      return -1;
  }
  else {
    printf("ReleaseKeyID: Ref %u, KeyID %u\n", sharedKeyID[flow->keyID].refCount, flow->keyID);
  }
  return 0;
}

/**
 * Get a KeySetID value from the KeySetID pool. Used by the 
 * flowmatcher command 
 */

int GetKeySetID(uint8_t *pList_queues, uint8_t nb_queues, uint8_t forwardPort, uint8_t *keySetID)
{
  int i;
  int j;

  // We need a new one keySetID
  for (i = 4; i < NUMBER_OF_KEY_SET_IDS; i++) {
    if (sharedKeySetID[i].refCount == 0) {
      sharedKeySetID[i].refCount = 1;
       sharedKeySetID[i].forwardPort = forwardPort;
       sharedKeySetID[i].nb_queues = nb_queues;
       for (j = 0; j < nb_queues; j++) {
         sharedKeySetID[i].list_queues[j] = pList_queues[j];
       }
       printf("New KeySetID %u\n", i);
       *keySetID = i;
       return 0;
    }
  }
  *keySetID = 0;
  return -1;
}

int ReuseKeySetID(uint8_t *pList_queues, uint8_t nb_queues, uint8_t forwardPort, uint8_t *keySetID)
{
  int i;
  int j;

  for (i = 4; i < NUMBER_OF_KEY_SET_IDS; i++) {
    //printf("ReuseKeySetID: %u, nb %u = %u, fport %u = %u\n", i, nb_queues, sharedKeySetID[i].nb_queues, forwardPort, sharedKeySetID[i].forwardPort);
    if (sharedKeySetID[i].refCount > 0 && sharedKeySetID[i].forwardPort == forwardPort && sharedKeySetID[i].nb_queues == nb_queues) {
      for (j = 0; j < nb_queues; j++) {
        printf("Queues %u = %u\n", sharedKeySetID[i].list_queues[j], pList_queues[j]);
        if (sharedKeySetID[i].list_queues[j] != pList_queues[j]) {
          break;
        }
        //printf("<<<<<<<<< %u = %u\n", j, (nb_queues - 1));
        if (j == (nb_queues - 1)) {
          // We have a match. Reuse KeySetID
          sharedKeySetID[i].refCount++;
          printf("Reuse KeySetID %u\n", i);
          *keySetID = i;
          return 0;
        }
      }
    }
  }
  printf("No KeySetID for reuse\n");
  *keySetID = 0;
  return -1;
}

int ReleaseKeySetID(struct pmd_internals *internals __rte_unused, struct rte_flow *flow)
{
  if (flow->keySetID >= NUMBER_OF_KEY_SET_IDS) {
    printf("Failed destroying flow - KeySetID %u illegal\n", flow->keySetID);
    return -1;
  }
  
  if (sharedKeySetID[flow->keySetID].refCount == 0) {
    printf("Failed destroying flow - KeySetID %u not in use\n", flow->keySetID);
    return -1;
  }

  sharedKeySetID[flow->keySetID].refCount--;

  if (sharedKeySetID[flow->keySetID].refCount == 0) {
    printf("ReleaseKeySetID: KeySetID %u released\n", flow->keySetID);
  }
  else {
    printf("ReleaseKeySetID: Ref %u, KeySetID %u\n", sharedKeySetID[flow->keySetID].refCount, flow->keySetID);
  }
  return 0;
}

int ReuseNtplID(uint8_t port, uint8_t priority, uint8_t keyID, uint8_t keySetID, uint32_t *ntplID)
{
  int i;
  for (i = 0; i < NUMBER_OF_NTPL_IDS; i++) {
    //printf("ReuseNtplID %u ref %u, ntpl %u, port %u=%u, pri %u=%u, KeyID %u=%u, keySetID %u=%u\n", i,
    //       sharedNtplID[i].refCount,
    //       sharedNtplID[i].ntplID,
    //       port,
    //       sharedNtplID[i].port,
    //       priority,
    //       sharedNtplID[i].priority,
    //       keyID,
    //       sharedNtplID[i].keyID,
    //       keySetID,
    //       sharedNtplID[i].keySetID);
    if (sharedNtplID[i].refCount > 0 &&
        port == sharedNtplID[i].port && 
        priority == sharedNtplID[i].priority && 
        keyID == sharedNtplID[i].keyID && 
        keySetID == sharedNtplID[i].keySetID) {
      sharedNtplID[i].refCount++;
      printf("Reuse ntplID %u\n", sharedNtplID[i].ntplID);
      *ntplID = sharedNtplID[i].ntplID;
      return 0;
    }
  }
  printf("No NTPL ID for reuse\n");
  *ntplID = 0;
  return -1;
}

int ReleaseNtplID(struct pmd_internals *internals, struct rte_flow *flow)
{
  int i;
  for (i = 0; i < NUMBER_OF_NTPL_IDS; i++) {
    if (sharedNtplID[i].ntplID == flow->ntplID) {
      if (sharedNtplID[i].refCount == 0) {
        printf("ReleaseNtplID: %u is not active - ????????\n", flow->ntplID);
        return -i;
      }
      sharedNtplID[i].refCount--;
      if (sharedNtplID[i].refCount == 0) {
        // NTPL ID is not in use anymore
        printf("ReleaseNtplID: Deleting %u\n", flow->ntplID);
        if (DeleteNtplID(internals, flow->ntplID) != 0)
          return -1;
        sharedNtplID[i].ntplID = 0;
        return 0;
      }
      printf("ReleaseNtplID: Releasing Ref: %u %u\n", sharedNtplID[i].refCount, flow->ntplID);
      return 0;
    }
  }
  printf("ReleaseNtplID: %u is not found - ????????\n", flow->ntplID);
  return -i;
}

void UpdateNtplIDFlowmatcher(uint32_t ntplID, uint8_t port, uint8_t priority, uint8_t keyID, uint8_t keySetID)
{
  int i;
  for (i = 0; i < NUMBER_OF_NTPL_IDS; i++) {
    //printf("UpdateNtplIDFlowmatcher%u ref %u, ntpl %u, port %u=%u, pri %u=%u, KeyID %u=%u, keySetID %u=%u\n", i,
    //         sharedNtplID[i].refCount,
    //         sharedNtplID[i].ntplID,
    //         port,
    //         sharedNtplID[i].port,
    //         priority,
    //         sharedNtplID[i].priority,
    //         keyID,
    //         sharedNtplID[i].keyID,
    //         keySetID,
    //         sharedNtplID[i].keySetID);
    if (sharedNtplID[i].refCount == 0) {
      sharedNtplID[i].refCount = 1;
      sharedNtplID[i].priority = priority;
      sharedNtplID[i].keyID = keyID; 
      sharedNtplID[i].keySetID = keySetID;
      sharedNtplID[i].ntplID = ntplID;
      sharedNtplID[i].port = port;
      break;
    }
  }
}

/**
 * Release a KeySetID value to the KeySetID pool. Used by the 
 * flowmatcher command 
 */
//static void PutKeySetID(struct pmd_internals *internals)
//{
//  int i;
//  pthread_mutex_lock(&internals->shm->mutex);
//  internals->shm->keySetID[i] = 0;
//  pthread_mutex_unlock(&internals->shm->mutex);
//}

//#define DUMP_FLOWS
#ifdef DUMP_FLOWS
static void DumpFlows(struct pmd_internals *internals)
{
  struct rte_flow *pFlow;
  struct filter_flow *pNtlpid;
  unsigned i = 0;
  printf("Dump flows\n");
  printf("----------\n");
  LIST_FOREACH(pFlow, &internals->flows, next) {
    unsigned j = 0;
    printf("Flow no %u\n", i++);
    printf("P %u, K %u, M %016llX. Queues:", pFlow->port, pFlow->key, (long long unsigned int)pFlow->typeMask);
    for (j = 0; j < pFlow->nb_queues; j++) {
      printf(" %u", pFlow->list_queues[j]);
    }
    printf("\nNTPL ID: Assign = %u", pFlow->assign_ntpl_id);
    LIST_FOREACH(pNtlpid, &pFlow->ntpl_id, next) {
      printf(" %u", pNtlpid->ntpl_id);
    }
    printf("\n---------------------------------------------------\n");
  }
}
#endif

void DumpFlow(struct rte_flow *pFlow)
{
  int i;

  printf("Flow: NTPL %u, KeyID %u, KeySetID %u, ipProto %u: ", 
         pFlow->ntplID,
         pFlow->keyID,
         pFlow->keySetID, 
         pFlow->ipProto);
  for (i = 0; i < 40; i++) {
    printf("%02X", pFlow->keyData[i]);
  }
  printf("\n");

  for (i = 0; i < NUMBER_OF_NTPL_IDS; i++) {
    if (sharedNtplID[i].ntplID == pFlow->ntplID) {
      printf("NTPL: ref %u, port %u, pri %u, KeyID %u, keySetID %u\n", sharedNtplID[i].refCount, sharedNtplID[i].port, sharedNtplID[i].priority, sharedNtplID[i].keyID, sharedNtplID[i].keySetID);
      break;
    }
  }

  printf("KeyID: ref %u, mask %08llX\n", sharedKeyID[pFlow->keyID].refCount, (long long unsigned int)sharedKeyID[pFlow->keyID].typeMask);

  printf("KeySetID: ref %u, nb %u, fport %u: ", sharedKeySetID[pFlow->keySetID].refCount, sharedKeySetID[pFlow->keySetID].nb_queues, sharedKeySetID[pFlow->keySetID].forwardPort);
  for (i = 0; i < sharedKeySetID[pFlow->keySetID].nb_queues; i++) {
    printf("%u ", sharedKeySetID[pFlow->keySetID].list_queues[i]);
  }
  printf("\n");
}


inline void fillKeyData(uint8_t *pKeyData, uint8_t *pData, int offset, int size)
{
  int i;
  for (i = 0; i < size; i++) {
      pKeyData[offset+i] = pData[(size - 1) - i];
  }
}
#define FILL_KEYDATA(a, b, c) { fillKeyData(a, (uint8_t *)&b, c, sizeof(b)); c += sizeof(b); }

inline void fillKeyData128(uint8_t *pKeyData, uint8_t *pData, int offset)
{
  int i;
  for (i = 0; i < 16; i++) {
      pKeyData[offset+i] = pData[i];
  }
}
#define FILL_KEYDATA128(a, b, c) { fillKeyData128(a, b, c); c += 16; }


static void dumpIPv4(const char *str, uint32_t ipv4)
{
  uint8_t *ptr = (uint8_t *)&ipv4;
  printf("IPv4 %s: %u.%u.%u.%u\n", str, ptr[3], ptr[2], ptr[1], ptr[0]);
}

static void dumpIPv6(const char *str, uint8_t *p)
{
  printf("IPv6 %s: [%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X]\n", str, IPV6_ADDRESS(p));
}

static void dumpPort(const char *str, uint16_t port)
{
  printf("Port %s: %u\n", str, port);
}

static void dumpFlowMatcher(NtFlow_t *pFlow)
{
  int i;
  printf("FlowMatcher: ID %llu, ", (long long unsigned int)pFlow->id);
  printf("KeyID %u, ",pFlow->keyId);
  printf("KeySetID %u, ",pFlow->keySetId);
  printf("op %u, ", pFlow->op);
  //printf("GFI %u, ", pFlow->gfi);
  //printf("TAU %u, ", pFlow->tau);
  printf("color %u, ", pFlow->color);
  printf("ipProto %u: ", pFlow->ipProtocolField);
  for (i = 0; i < 40; i++) {
    printf("%02X", pFlow->keyData[i]);
  }
  printf("\n");
}

int CreateOptimizedFilterFlowmatcher1(struct pmd_internals *internals,
                                      uint64_t typeMask,
                                      uint8_t keyID,
                                      struct color_s *pColor,
                                      struct rte_flow_error *error)
{
  struct filter_values_s *pFilter_values;
  int iRet = 0;
  bool first = true;
  char *filter_buffer = NULL;
  uint32_t ntplID1;
  uint32_t ntplID2;

#ifdef DUMP_FLOWS
  DumpFlows(internals);
#endif

  NTACC_LOCK(&internals->lock);
  if (LIST_EMPTY(&internals->filter_values)) {
    NTACC_UNLOCK(&internals->lock);
    return 0;
  }

  /*************************************************************/
  /*          Make the keytype and keydef commands             */
  /*************************************************************/
  filter_buffer = rte_malloc(internals->name, NTPL_BSIZE + 1, 0);
  if (!filter_buffer) {
    iRet = -1;
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Allocating memory failed");
    goto Errors;
  }

  /*************************************************************/
  /*                Make the keytype command                   */
  /*************************************************************/
  first = true;
  LIST_FOREACH(pFilter_values, &internals->filter_values, next) {
    if (first) {
      if (pColor->type == ONE_COLOR) {
        snprintf(filter_buffer, NTPL_BSIZE, "keytype[name=ktflow%u;colorinfo=true;tag=%s]={", keyID, internals->tagName);
      }
      else {
        snprintf(filter_buffer, NTPL_BSIZE, "keytype[name=ktflow%u;tag=%s]={", keyID, internals->tagName);
      }
      first=false;
    }
    else {
      snprintf(&filter_buffer[strlen(filter_buffer)], NTPL_BSIZE - strlen(filter_buffer) - 1, ",");
    }
    snprintf(&filter_buffer[strlen(filter_buffer)], NTPL_BSIZE - strlen(filter_buffer) - 1, "%u", pFilter_values->size);
  }
  snprintf(&filter_buffer[strlen(filter_buffer)],  NTPL_BSIZE - strlen(filter_buffer) - 1, "}");

  if (DoNtpl(filter_buffer, &ntplID1, internals, error)) {
    iRet = -1;
    goto Errors;
  }

  /*************************************************************/
  /*                Make the keydef command                    */
  /*************************************************************/
  first = true;
  LIST_FOREACH(pFilter_values, &internals->filter_values, next) {
    if (first) {


      if (typeMask & (TCP_SRC_PORT | TCP_DST_PORT | UDP_SRC_PORT | UDP_DST_PORT | SCTP_SRC_PORT | SCTP_DST_PORT)) {
        snprintf(filter_buffer, NTPL_BSIZE, "keydef[name=kdflow%u;keytype=ktflow%u;ipprotocolfield=outer;tag=%s]=(", keyID, keyID, internals->tagName);
      }
      else {
        snprintf(filter_buffer, NTPL_BSIZE, "keydef[name=kdflow%u;keytype=ktflow%u;ipprotocolfield=none;tag=%s]=(", keyID, keyID, internals->tagName);
      }
      first=false;
    }
    else {
      snprintf(&filter_buffer[strlen(filter_buffer)], NTPL_BSIZE - strlen(filter_buffer) - 1, ",");
    }

    if (pFilter_values->size == 128) {
      if (pFilter_values->layer == LAYER2) {
        // This is an ethernet address
        snprintf(&filter_buffer[strlen(filter_buffer)], NTPL_BSIZE - strlen(filter_buffer) - 1,
                "{0xFFFFFFFFFFFFFFFFFFFFFFFF00000000:%s[%u]/%u}", pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
      if (NON_ZERO16(pFilter_values->value.v128.maskVal)) {
        snprintf(&filter_buffer[strlen(filter_buffer)], NTPL_BSIZE - strlen(filter_buffer) - 1,
                "{0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X:%s[%u]/%u}", 
                 IPV6_ADDRESS(pFilter_values->value.v128.maskVal), pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
      else {
        snprintf(&filter_buffer[strlen(filter_buffer)],  NTPL_BSIZE - strlen(filter_buffer) - 1,
                "%s[%u]/%u", pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
    }
    else {
      if (pFilter_values->mask == 0) {
        switch (pFilter_values->size)
        {
        case 16:
          if (pFilter_values->value.v16.maskVal) {
            pFilter_values->mask = pFilter_values->value.v16.maskVal;
          }
          break;
        case 32:
          if (pFilter_values->value.v32.maskVal) {
            pFilter_values->mask = pFilter_values->value.v32.maskVal;
          }
          break;
        case 64:
          if (pFilter_values->value.v64.maskVal) {
            pFilter_values->mask = pFilter_values->value.v64.maskVal;
          }
          break;
        }
      }
      if (pFilter_values->mask != 0) {
        snprintf(&filter_buffer[strlen(filter_buffer)],  NTPL_BSIZE - strlen(filter_buffer) - 1,
                 "{0x%llX:%s[%u]/%u}", (const long long unsigned int)pFilter_values->mask, pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
      else {
        snprintf(&filter_buffer[strlen(filter_buffer)],  NTPL_BSIZE - strlen(filter_buffer) - 1,
                "%s[%u]/%u", pFilter_values->layerString, pFilter_values->offset, pFilter_values->size);
      }
    }
  }

  snprintf(&filter_buffer[strlen(filter_buffer)],  NTPL_BSIZE - strlen(filter_buffer) - 1, ")");

  if (DoNtpl(filter_buffer, &ntplID2, internals, error)) {
    iRet = -1;
    goto Errors;
  }
  UpdateKeyIDNtplID(keyID, ntplID1, ntplID2);
  
Errors:
  NTACC_UNLOCK(&internals->lock);

  if (filter_buffer) {
    rte_free(filter_buffer);
  }
  return iRet;
}

int CreateOptimizedFilterFlowmatcher2(struct pmd_internals *internals,
                                      struct rte_flow *flow,
                                      uint8_t keyID,
                                      uint8_t keySetID,
                                      uint64_t typeMask,
                                      struct color_s *pColor,
                                      struct rte_flow_error *error)
{
  NtFlow_t flowMatcher;
  int dataIndex;
  struct filter_values_s *pFilter_values = NULL;
  int iRet = 0;
  bool first = true;

  NTACC_LOCK(&internals->lock);
  if (LIST_EMPTY(&internals->filter_values)) {
    NTACC_UNLOCK(&internals->lock);
    return 0;
  }

  /*************************************************************/
  /*                Program flows                              */
  /*************************************************************/


  memset(&flowMatcher, 0, sizeof(NtFlow_t));
  dataIndex = 0;

  while (!LIST_EMPTY(&internals->filter_values)) {
    pFilter_values = LIST_FIRST(&internals->filter_values);
    LIST_REMOVE(pFilter_values, next);
    if (first) {
      if (pColor->type == ONE_COLOR)
        flowMatcher.color = pColor->color;
      else
        flowMatcher.color = 0;

      flowMatcher.keyId = keyID;
      flowMatcher.keySetId = keySetID;
      flowMatcher.op = 1;

      if (typeMask & (TCP_SRC_PORT | TCP_DST_PORT)) {
        flowMatcher.ipProtocolField = 6;
      }
      else if (typeMask & (UDP_SRC_PORT | UDP_DST_PORT)) {
        flowMatcher.ipProtocolField = 17;
      }
      else if (typeMask & (SCTP_SRC_PORT | SCTP_DST_PORT)) {
        flowMatcher.ipProtocolField = 132;
      }
      first=false;
    }

    switch (pFilter_values->size)
    {
    case 16:
      if (dataIndex > 38) {
        rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Flow manager programming overrun. To many data");
        iRet = -1;
        break;
      }
      if (pFilter_values->layer == LAYER4) {
        if (pFilter_values->offset == 0) {
          dumpPort("SRC", pFilter_values->value.v16.specVal);
        }
        else {
          dumpPort("DST", pFilter_values->value.v16.specVal);
        }
      }
      FILL_KEYDATA(flowMatcher.keyData, pFilter_values->value.v16.specVal, dataIndex);
      break;
    case 32:
      if (dataIndex > 36) {
        rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Flow manager programming overrun. To many data");
        iRet = -1;
        break;
      }
      if (pFilter_values->layer == LAYER3) {
        if (pFilter_values->offset == 12) {
          dumpIPv4("SRC", pFilter_values->value.v32.specVal);
        }
        else {
          dumpIPv4("DST", pFilter_values->value.v32.specVal);
        }
      }
      FILL_KEYDATA(flowMatcher.keyData, pFilter_values->value.v32.specVal, dataIndex);
      break;
    case 64:
      if (dataIndex > 32) {
        rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Flow manager programming overrun. To many data");
        iRet = -1;
        break;
      }
      FILL_KEYDATA(flowMatcher.keyData, pFilter_values->value.v64.specVal, dataIndex);
      break;
    case 128:
      if (dataIndex > 24) {
        rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Flow manager programming overrun. To many data");
        iRet = -1;
        break;
      }
      if (pFilter_values->layer == LAYER2) {
        //snprintf(&filter_buffer1[strlen(filter_buffer1)], NTPL_BSIZE - strlen(filter_buffer1) - 1, "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
        //         MAC_ADDRESS2(pFilter_values->value.v128.specVal));
      }
      else {
        if (pFilter_values->layer == LAYER3) {
          if (pFilter_values->offset == 8) {
            dumpIPv6("SRC", pFilter_values->value.v128.specVal);
          }
          else {
            dumpIPv6("DST", pFilter_values->value.v128.specVal);
          }
        }
        FILL_KEYDATA128(flowMatcher.keyData, pFilter_values->value.v128.specVal, dataIndex);
      }
      break;
    }
    if (pFilter_values) {
      rte_free(pFilter_values);
    }
  }
  //dumpFlowMatcher(&flowMatcher);

  dumpFlowMatcher(&flowMatcher);
  if (NT_FlowWrite(internals->hFlowStream, &flowMatcher, 1000) != NT_SUCCESS) {
    rte_flow_error_set(error, EIO, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Flow manager programming failed");
    printf("<==error error error error error error error error error error error error==>\n");
    iRet = -1;
  }

  memcpy(flow->keyData, flowMatcher.keyData, 40);
  flow->ipProto = flowMatcher.ipProtocolField;
  NTACC_UNLOCK(&internals->lock);
  return iRet;
}

int UnlearnFlowFlowmatcher(struct pmd_internals *internals,
                           struct rte_flow *flow)
{
  NtFlow_t flowMatcher;

  memset(&flowMatcher, 0, sizeof(NtFlow_t));

  // Delete flow in FPGA
  flowMatcher.keyId = flow->keyID;
  flowMatcher.keySetId = flow->keySetID;
  flowMatcher.ipProtocolField = flow->ipProto;
  flowMatcher.op = 0;
  memcpy(flowMatcher.keyData, flow->keyData, 40);

  if (NT_FlowWrite(internals->hFlowStream, &flowMatcher, 1000) != NT_SUCCESS) {
    printf("<==error error error error error error error error error error error error==>\n");
    return -1;
  }

  //{
  //  NtFlowStatus_t flowStatus;
  //  if (NT_FlowStatusRead(internals->hFlowStream, &flowStatus) == NT_SUCCESS) {
  //    switch (flowStatus.flags) {
  //    case NT_FLOW_STAT_LFS:  /*< Learn fail status flag */
  //      printf("===========> Learn failed\n");
  //      break;
  //    case NT_FLOW_STAT_LIS:  /*< Learn ignore status flag */
  //      printf("===========> Learn ignore\n");
  //      break;
  //    case NT_FLOW_STAT_UIS:  /*< Un-learn ignore status flag */
  //      printf("===========> Unlearn ignore\n");
  //      break;
  //    default:
  //      break;
  //    }
  //  }
  //}

  return 0;
}


