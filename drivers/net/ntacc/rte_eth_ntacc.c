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
#include <time.h>
#include <linux/limits.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_vdev.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_version.h>
#include <net/if.h>
#include <nt.h>

#include "rte_eth_ntacc.h"
#include "filter_ntacc.h"

#define ETH_NTACC_PORT_ARG "port"
#define ETH_NTACC_NTPL_ARG "ntpl"

#define HW_MAX_PKT_LEN  10000
#define HW_MTU    (HW_MAX_PKT_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN) /**< MTU */

#define MAX_NTACC_PORTS 32
#define STREAMIDS_PER_PORT  (256 / internals->nbPorts)

#define COLOR_PRESENT(a) ((a) & 0x2000)
#define COLOR_CHECK(a)   ((a) > 0x1FFF)
#define GET_COLOR(a) ((uint32_t)((a) & 0x1FFF))
#define SET_COLOR(a) ((a) | 0x2000)
#define GET_HASH(a)  ((uint32_t)((((uint64_t)(a)) >> 14) & 0xFFFFFF))

#define MAX_NTPL_NAME 512

static struct {
   int32_t major;
   int32_t minor;
   int32_t patch;
} supportedDriver = {3, 6, 6};

#define NB_SUPPORTED_FPGAS 7
struct {
  uint32_t item:12;
  uint32_t product:16;
  uint32_t ver:8;
  uint32_t rev:8;
  uint32_t build:10;
} supportedAdapters[NB_SUPPORTED_FPGAS] =
{
  { 200, 9500, 8, 6, 0 },
  { 200, 9501, 8, 6, 0 },
  { 200, 9502, 8, 6, 0 },
  { 200, 9503, 8, 6, 0 },
  { 200, 9505, 8, 6, 0 },
  { 200, 9508, 7, 6, 0 },
  { 200, 9512, 8, 8, 0 },
};

static void *_libnt;

/* NTAPI library functions */
int (*_NT_Init)(uint32_t);
int (*_NT_NetRxOpen)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t, int);
int (*_NT_NetRxGet)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxRelease)(NtNetStreamRx_t, NtNetBuf_t);
int (*_NT_NetRxClose)(NtNetStreamRx_t);
char *(*_NT_ExplainError)(int, char *, uint32_t);
int (*_NT_NetTxOpen)(NtNetStreamTx_t *, const char *, uint64_t, uint32_t, uint32_t);
int (*_NT_NetTxClose)(NtNetStreamTx_t);
int (*_NT_InfoOpen)(NtInfoStream_t *, const char *);
int (*_NT_InfoRead)(NtInfoStream_t, NtInfo_t *);
int (*_NT_InfoClose)(NtInfoStream_t);
int (*_NT_ConfigOpen)(NtConfigStream_t *, const char *);
int (*_NT_ConfigClose)(NtConfigStream_t);
int (*_NT_NTPL)(NtConfigStream_t, const char *, NtNtplInfo_t *, uint32_t);
int (*_NT_NetRxGetNextPacket)(NtNetStreamRx_t, NtNetBuf_t *, int);
int (*_NT_NetRxOpenMulti)(NtNetStreamRx_t *, const char *, enum NtNetInterface_e, uint32_t *, unsigned int, int);
int (*_NT_NetTxRelease)(NtNetStreamTx_t, NtNetBuf_t);
int (*_NT_NetTxGet)(NtNetStreamTx_t, NtNetBuf_t *, uint32_t, size_t, enum NtNetTxPacketOption_e, int);
int (*_NT_NetTxAddPacket)(NtNetStreamTx_t hStream, uint32_t port, NtNetTxFragment_t *fragments, uint32_t fragmentCount, int timeout );
int (*_NT_StatClose)(NtStatStream_t);
int (*_NT_StatOpen)(NtStatStream_t *, const char *);
int (*_NT_StatRead)(NtStatStream_t, NtStatistics_t *);

static int _dev_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error __rte_unused);

static char errorBuffer[1024];

static int first = 1;

static volatile uint16_t port_locks[MAX_NTACC_PORTS];

static const char *valid_arguments[] = {
  ETH_NTACC_PORT_ARG,
  ETH_NTACC_NTPL_ARG,
  NULL
};

static struct ether_addr eth_addr[MAX_NTACC_PORTS];

static inline void priv_lock(struct pmd_internals *internals)
{
  rte_spinlock_lock(&internals->lock);
}

static inline void priv_unlock(struct pmd_internals *internals)
{
  rte_spinlock_unlock(&internals->lock);
}

static void _write_to_file(int fd, const char *buffer)
{
  if (write(fd, buffer, strlen(buffer)) < 0) {
    RTE_LOG(ERR, PMD, "NTPL dump failed: Unable to write to file. Error %d\n", errno);
  }
}

int DoNtpl(const char *ntplStr, NtNtplInfo_t *ntplInfo, struct pmd_internals *internals)
{
  NtConfigStream_t hCfgStream;      // Config stream
  int status;
  int fd;

  if((status = (*_NT_ConfigOpen)(&hCfgStream, "capture")) != NT_SUCCESS) {
    // Get the status code as text
    (*_NT_ExplainError)(status, errorBuffer, sizeof(errorBuffer)-1);
    fprintf(stderr, "NT_ConfigOpen() failed: %s\n", errorBuffer);
    return -1;
  }

  if (internals->ntpl_file) {
    fd = open(internals->ntpl_file, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) {
      RTE_LOG(ERR, PMD, "NTPL dump failed: Unable to open file %s. Error %d\n", internals->ntpl_file, errno);
    }
    _write_to_file(fd, ntplStr); _write_to_file(fd, "\n");
    close(fd);
  }

  RTE_LOG(DEBUG, PMD, "NTPL : %s\n", ntplStr);
  if((status = (*_NT_NTPL)(hCfgStream, ntplStr, ntplInfo, NT_NTPL_PARSER_VALIDATE_NORMAL)) != NT_SUCCESS) {
    // Get the status code as text
    (*_NT_ExplainError)(status, errorBuffer, sizeof(errorBuffer)-1);
    RTE_LOG(ERR, PMD, "NT_NTPL() failed: %s\n", errorBuffer);
    RTE_LOG(ERR, PMD, ">>> NTPL errorcode: %X\n", ntplInfo->u.errorData.errCode);
    RTE_LOG(ERR, PMD, ">>> %s\n", ntplInfo->u.errorData.errBuffer[0]);
    RTE_LOG(ERR, PMD, ">>> %s\n", ntplInfo->u.errorData.errBuffer[1]);
    RTE_LOG(ERR, PMD, ">>> %s\n", ntplInfo->u.errorData.errBuffer[2]);
    (*_NT_ConfigClose)(hCfgStream);

    if (internals->ntpl_file) {
      fd = open(internals->ntpl_file, O_WRONLY | O_APPEND | O_CREAT, 0666);
      if (fd == -1) {
        RTE_LOG(ERR, PMD, "NT_NTPL() dump failed: Unable to open file %s. Error %d\n", internals->ntpl_file, errno);
      }
      _write_to_file(fd, ntplInfo->u.errorData.errBuffer[0]); _write_to_file(fd, "\n");
      _write_to_file(fd, ntplInfo->u.errorData.errBuffer[1]); _write_to_file(fd, "\n");
      _write_to_file(fd, ntplInfo->u.errorData.errBuffer[2]); _write_to_file(fd, "\n");
      close(fd);
    }
    return -1;
  }
  RTE_LOG(DEBUG, PMD, "NTPL : %d\n", ntplInfo->ntplId);
  (*_NT_ConfigClose)(hCfgStream);
  return 0;
}

static int eth_ntacc_rx_jumbo(struct rte_mempool *mb_pool,
                              struct rte_mbuf *mbuf,
                              const u_char *data,
                              uint16_t data_len)
{
  struct rte_mbuf *m = mbuf;

  /* Copy the first segment. */
  uint16_t len = rte_pktmbuf_tailroom(mbuf);

  rte_memcpy(rte_pktmbuf_append(mbuf, len), data, len);
  data_len -= len;
  data += len;

  while (data_len > 0) {
    /* Allocate next mbuf and point to that. */
    m->next = rte_pktmbuf_alloc(mb_pool);

    if (unlikely(!m->next))
      return -1;

    m = m->next;

    /* Headroom is not needed in chained mbufs. */
    rte_pktmbuf_prepend(m, rte_pktmbuf_headroom(m));
    m->pkt_len = 0;
    m->data_len = 0;

    /* Copy next segment. */
    len = RTE_MIN(rte_pktmbuf_tailroom(m), data_len);
    rte_memcpy(rte_pktmbuf_append(m, len), data, len);

    mbuf->nb_segs++;
    data_len -= len;
    data += len;
  }
  return mbuf->nb_segs;
}

static uint16_t eth_ntacc_rx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  struct ntacc_rx_queue *rx_q = queue;

  if (unlikely(rx_q->pNetRx == NULL || nb_pkts == 0))
    return 0;

  // Do we have any segment
  if (rx_q->pSeg == NULL) {
    if ((*_NT_NetRxGet)(rx_q->pNetRx, &rx_q->pSeg, 0) != NT_SUCCESS) {
      if (rx_q->pSeg != NULL) {
        (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
        rx_q->pSeg = NULL;
      }
      return 0;

    }
    if (NT_NET_GET_SEGMENT_LENGTH(rx_q->pSeg)) {
      // Build a packet structure
      _nt_net_build_pkt_netbuf(rx_q->pSeg, &rx_q->pkt);
    } else {
      (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
      rx_q->pSeg = NULL;
      return 0;
    }
  }

  if (rte_mempool_get_bulk(rx_q->mb_pool, (void **)bufs, nb_pkts) != 0)
    return 0;

  uint16_t i, num_rx = 0;
  for (i = 0; i < nb_pkts; i++) {
    struct rte_mbuf *mbuf = bufs[i];
    rte_mbuf_refcnt_set(mbuf, 1);
    rte_pktmbuf_reset(mbuf);

   NtDyn2Descr_t *dyn2 = _NT_NET_GET_PKT_DESCR_PTR_DYN2(&rx_q->pkt);

    const uint64_t color = dyn2->color;
    if (COLOR_PRESENT(color)) {
      mbuf->hash.fdir.hi = GET_COLOR(color);
      mbuf->ol_flags |= PKT_RX_FDIR_ID;
    }
    else {
      mbuf->hash.rss = GET_HASH(color);
      mbuf->ol_flags |= PKT_RX_RSS_HASH;
    }

    mbuf->port = rx_q->in_port;

    const uint16_t data_len = (uint16_t)(dyn2->capLength - dyn2->descrLength - 4);
    if (data_len <= rx_q->buf_size) {
      /* Packet will fit in the mbuf, go ahead and copy */
      mbuf->pkt_len = mbuf->data_len = data_len;
      rte_memcpy((u_char *)mbuf->buf_addr + RTE_PKTMBUF_HEADROOM, (uint8_t*)dyn2 + dyn2->descrLength, mbuf->data_len);
    } else {
      /* Try read jumbo frame into multi mbufs. */
      if (unlikely(eth_ntacc_rx_jumbo(rx_q->mb_pool, mbuf, (uint8_t*)dyn2 + dyn2->descrLength, data_len) == -1))
        break;
    }

    num_rx++;

    /* Get the next packet if any */
    if (_nt_net_get_next_packet(rx_q->pSeg,
        NT_NET_GET_SEGMENT_LENGTH(rx_q->pSeg),
        &rx_q->pkt) == 0 ) {
      (*_NT_NetRxRelease)(rx_q->pNetRx, rx_q->pSeg);
      rx_q->pSeg = NULL;
      break;
    }
  }

#ifdef USE_SW_STAT
  rx_q->rx_pkts+=num_rx;
#endif

  if (num_rx < nb_pkts) {
    rte_mempool_put_bulk(rx_q->mb_pool, (void * const *)(bufs + num_rx), nb_pkts-num_rx);
  }
  return num_rx;
}

/*
 * Callback to handle sending packets through a real NIC.
 */
static uint16_t eth_ntacc_tx(void *queue,
                             struct rte_mbuf **bufs,
                             uint16_t nb_pkts)
{
  unsigned i;
  int ret;
  struct ntacc_tx_queue *tx_q = queue;

  if (unlikely(tx_q == NULL || tx_q->pNetTx == NULL || nb_pkts == 0)) {
    return 0;
  }

  for (i = 0; i < nb_pkts; i++) {
    struct rte_mbuf *mbuf = bufs[i];
    struct NtNetTxFragment_s frag[10]; // Need fragments enough for a jumbo packet */
    uint8_t fragCnt = 0;
    frag[fragCnt].data = rte_pktmbuf_mtod(mbuf, u_char *);
    frag[fragCnt++].size = mbuf->data_len;
    if (unlikely(mbuf->nb_segs > 1)) {
      while (mbuf->next) {
        mbuf = mbuf->next;
        frag[fragCnt].data = rte_pktmbuf_mtod(mbuf, u_char *);
        frag[fragCnt++].size = mbuf->data_len;
      }
    }
    ret = (*_NT_NetTxAddPacket)(tx_q->pNetTx, tx_q->port, frag, fragCnt, 0);
    if (unlikely(ret != NT_SUCCESS)) {
      /* unsent packets is not expected to be freed */
#ifdef USE_SW_STAT
      tx_q->err_pkts += (nb_pkts - i);
#endif
      break;
    }
    rte_pktmbuf_free(bufs[i]);
  }
#ifdef USE_SW_STAT
      tx_q->tx_pkts += i;
#endif

  return i;
}

static int eth_dev_start(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  struct ntacc_rx_queue *rx_q = internals->rxq;
  struct ntacc_tx_queue *tx_q = internals->txq;
  uint queue;
  int status;

#ifndef DO_NOT_CREATE_DEFAULT_FILTER
  uint8_t nb_queues = 0;
  NtNtplInfo_t ntplInfo;
  char *ntpl_buf = NULL;
  uint8_t list_queues[256];
#endif

#ifndef DO_NOT_CREATE_DEFAULT_FILTER
  // Build default flow
  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      list_queues[nb_queues++] = queue;
    }
  }

  ntpl_buf = malloc(4096);
  if (!ntpl_buf) {
    RTE_LOG(ERR, PMD, "Out of memory in eth_dev_start\n");
    goto StartError;
  }
  ntpl_buf[0] = 0;

  // Set the priority
  strcat(ntpl_buf, "assign[priority=62;Descriptor=DYN2;");

  if (internals->rss_hf != 0) {
    // Set the stream IDs
    CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, nb_queues, list_queues);
    // If RSS is used, then set the Hash mode
    if (CreateHash(internals->rss_hf, internals, NULL, 62) != 0) {
      RTE_LOG(ERR, PMD, "Failed to create hash function eth_dev_start\n");
      goto StartError;
    }
  }
  else {
    // Set the stream IDs
    CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, 1, list_queues);
  }

  // Set the port number
  sprintf(&ntpl_buf[strlen(ntpl_buf)], "]=port==%u", internals->port);

  if (DoNtpl(ntpl_buf, &ntplInfo, internals) != 0) {
    RTE_LOG(ERR, PMD, "Failed to create default filter in eth_dev_start\n");
    goto StartError;
  }
  if (ntpl_buf) {
    free(ntpl_buf);
    ntpl_buf = NULL;
  }
#endif

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      if ((status = (*_NT_NetRxOpen)(&rx_q[queue].pNetRx, "DPDK", NT_NET_INTERFACE_SEGMENT, rx_q[queue].stream_id, -1)) != NT_SUCCESS) {
        (*_NT_ExplainError)(status, errorBuffer, sizeof(errorBuffer));
        RTE_LOG(ERR, PMD, "NT_NetRxOpen() failed: %s\n", errorBuffer);
        goto StartError;
      }
    }
  }

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (tx_q[queue].enabled) {
      if ((status = (*_NT_NetTxOpen)(&tx_q[queue].pNetTx, "DPDK", 1 << tx_q[queue].port, -1, 0)) != NT_SUCCESS) {
        if ((status = (*_NT_NetTxOpen)(&tx_q[queue].pNetTx, "DPDK", 1 << tx_q[queue].port, -2, 0)) != NT_SUCCESS) {
          (*_NT_ExplainError)(status, errorBuffer, sizeof(errorBuffer));
          RTE_LOG(DEBUG, PMD, "NT_NetTxOpen(0x%X, -2, 0) failed: %s\n", 1 << tx_q[queue].port, errorBuffer);
          goto StartError;
        }
        RTE_LOG(DEBUG, PMD, "NT_NetTxOpen() Not optimal hostbuffer found on a neighbour numa node\n");
      }
    }
    tx_q[queue].plock = &port_locks[tx_q[queue].port];
  }

  dev->data->dev_link.link_status = 1;
  return 0;

StartError:
#ifndef DO_NOT_CREATE_DEFAULT_FILTER
  if (ntpl_buf) {
    free(ntpl_buf);
  }
#endif
  return -1;
}

/*
 * This function gets called when the current port gets stopped.
 * Is the only place for us to close all the tx streams dumpers.
 * If not called the dumpers will be flushed within each tx burst.
 */
static void eth_dev_stop(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  struct ntacc_rx_queue *rx_q = internals->rxq;
  struct ntacc_tx_queue *tx_q = internals->txq;
  struct rte_flow_error error;
  uint queue;

  _dev_flow_flush(dev, &error);

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (rx_q[queue].enabled) {
      if (rx_q[queue].pSeg) {
        (*_NT_NetRxRelease)(rx_q[queue].pNetRx, rx_q[queue].pSeg);
        rx_q[queue].pSeg = NULL;
      }
      if (rx_q[queue].pNetRx) {
          (void)(*_NT_NetRxClose)(rx_q[queue].pNetRx);
      }
    }
  }
  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (tx_q[queue].enabled) {
      if (tx_q[queue].pNetTx) {
        (void)(*_NT_NetTxClose)(tx_q[queue].pNetTx);
      }
    }
  }

  if (internals->hInfo) {
    (void)(*_NT_InfoClose)(internals->hInfo);
  }
#ifndef USE_SW_STAT
  if (internals->hStat) {
    (void)(*_NT_StatClose)(internals->hStat);
  }
#endif
  dev->data->dev_link.link_status = 0;
}

static int eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
    internals->rss_hf = dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf;
  }
  else {
    internals->rss_hf = 0;
  }
  return 0;
}

static void eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
  struct pmd_internals *internals = dev->data->dev_private;
  dev_info->if_index = internals->if_index;
  dev_info->driver_name = internals->driverName;
  dev_info->max_mac_addrs = 1;
  dev_info->max_rx_pktlen = HW_MTU;
  dev_info->max_rx_queues = STREAMIDS_PER_PORT > RTE_ETHDEV_QUEUE_STAT_CNTRS ? RTE_ETHDEV_QUEUE_STAT_CNTRS : STREAMIDS_PER_PORT;
  dev_info->max_tx_queues = STREAMIDS_PER_PORT > RTE_ETHDEV_QUEUE_STAT_CNTRS ? RTE_ETHDEV_QUEUE_STAT_CNTRS : STREAMIDS_PER_PORT;
  dev_info->min_rx_bufsize = 0;
}

#ifdef USE_SW_STAT
static void eth_stats_get(struct rte_eth_dev *dev,
                          struct rte_eth_stats *igb_stats)
{
  unsigned i;
  unsigned long rx_total = 0;
  unsigned long tx_total = 0;
  unsigned long tx_err_total = 0;
  const struct pmd_internals *internal = dev->data->dev_private;

  memset(igb_stats, 0, sizeof(*igb_stats));
  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    if (internal->rxq[i].enabled) {
      igb_stats->q_ipackets[i] = internal->rxq[i].rx_pkts;
      rx_total += igb_stats->q_ipackets[i];
    }
  }

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    if (internal->txq[i].enabled) {
      igb_stats->q_opackets[i] = internal->txq[i].tx_pkts;
      igb_stats->q_errors[i] = internal->txq[i].err_pkts;
      tx_total += igb_stats->q_opackets[i];
      tx_err_total += igb_stats->q_errors[i];
    }
  }

  igb_stats->ipackets = rx_total;
  igb_stats->opackets = tx_total;
  igb_stats->oerrors = tx_err_total;
}
#else
static void eth_stats_get(struct rte_eth_dev *dev,
                          struct rte_eth_stats *igb_stats)
{
  struct pmd_internals *internals = dev->data->dev_private;
  uint queue;
  int status;
  NtStatistics_t statData;
  uint8_t port;
  char errBuf[NT_ERRBUF_SIZE];

  memset(igb_stats, 0, sizeof(*igb_stats));

  /* port used */
  port = (uint8_t)internals->txq[0].port;

  /* Get stat data */
  statData.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
  statData.u.query_v2.poll=1;
  statData.u.query_v2.clear=0;
  if ((status = (*_NT_StatRead)(internals->hStat, &statData)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_StatRead failed. Code 0x%x = %s\n", status, errBuf);
    return;
  }

  igb_stats->ipackets = statData.u.query_v2.data.port.aPorts[port].rx.RMON1.pkts;
  igb_stats->ibytes = statData.u.query_v2.data.port.aPorts[port].rx.RMON1.octets;
  igb_stats->opackets = statData.u.query_v2.data.port.aPorts[port].tx.RMON1.pkts;
  igb_stats->obytes = statData.u.query_v2.data.port.aPorts[port].tx.RMON1.octets;
  igb_stats->imissed = statData.u.query_v2.data.port.aPorts[port].rx.extDrop.pktsOverflow;
  igb_stats->ierrors = statData.u.query_v2.data.port.aPorts[port].rx.RMON1.crcAlignErrors;
  igb_stats->oerrors = statData.u.query_v2.data.port.aPorts[port].tx.RMON1.crcAlignErrors;

  for (queue = 0; queue < RTE_ETHDEV_QUEUE_STAT_CNTRS; queue++) {
    if (internals->rxq[queue].enabled) {
      igb_stats->q_ipackets[queue] = statData.u.query_v2.data.stream.streamid[internals->rxq[queue].stream_id].forward.pkts;
      igb_stats->q_ibytes[queue] =  statData.u.query_v2.data.stream.streamid[internals->rxq[queue].stream_id].forward.octets;
    }
    else {
      igb_stats->q_ipackets[queue] = 0;
      igb_stats->q_ibytes[queue] = 0;
    }
  }
}
#endif

#ifdef USE_SW_STAT
static void eth_stats_reset(struct rte_eth_dev *dev)
{
  unsigned i;
  struct pmd_internals *internal = dev->data->dev_private;

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internal->rxq[i].rx_pkts = 0;
  }
  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internal->txq[i].tx_pkts = 0;
    internal->txq[i].err_pkts = 0;
  }
}
#else
static void eth_stats_reset(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  int status;
  NtStatistics_t statData;
  char errBuf[NT_ERRBUF_SIZE];

  statData.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
  statData.u.query_v2.poll=1;
  statData.u.query_v2.clear=1;
  if ((status = (*_NT_StatRead)(internals->hStat, &statData)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_StatRead failed. Code 0x%x = %s\n", status, errBuf);
    return;
  }
}
#endif

static void eth_dev_close(struct rte_eth_dev *dev __rte_unused)
{
}

static void eth_queue_release(void *q __rte_unused)
{
}

static int eth_link_update(struct rte_eth_dev *dev,
                           int wait_to_complete  __rte_unused)
{
  NtInfo_t info;
  uint status;
  char errBuf[NT_ERRBUF_SIZE];
  struct pmd_internals *internals = dev->data->dev_private;

  /* Find local port offset */
  info.cmd = NT_INFO_CMD_READ_PORT_V8;
  info.u.port_v8.portNo = (uint8_t)(internals->txq[0].port);
  if ((status = (*_NT_InfoRead)(internals->hInfo, &info)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_InfoRead failed. Code 0x%x = %s\n", status, errBuf);
    return status;
  }
  dev->data->dev_link.link_status = info.u.port_v8.data.state == NT_LINK_STATE_UP ? 1 : 0;
  switch (info.u.port_v8.data.speed) {
  case NT_LINK_SPEED_UNKNOWN:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10M:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_10M;
    break;
  case NT_LINK_SPEED_100M:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_100M;
    break;
  case NT_LINK_SPEED_1G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_10G;
    break;
  case NT_LINK_SPEED_40G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_40G;
    break;
  case NT_LINK_SPEED_50G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_50G;
    break;
  case NT_LINK_SPEED_100G:
    dev->data->dev_link.link_speed = ETH_SPEED_NUM_100G;
    break;
  }
  return 0;
}

static int eth_rx_queue_setup(struct rte_eth_dev *dev,
                              uint16_t rx_queue_id,
                              uint16_t nb_rx_desc __rte_unused,
                              unsigned int socket_id __rte_unused,
                              const struct rte_eth_rxconf *rx_conf __rte_unused,
                              struct rte_mempool *mb_pool)
{
  struct rte_pktmbuf_pool_private *mbp_priv;
  struct pmd_internals *internals = dev->data->dev_private;
  struct ntacc_rx_queue *rx_q = &internals->rxq[rx_queue_id];

  rx_q->mb_pool = mb_pool;
  dev->data->rx_queues[rx_queue_id] = rx_q;
  rx_q->in_port = dev->data->port_id;

  mbp_priv =  rte_mempool_get_priv(rx_q->mb_pool);
  rx_q->buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);
  rx_q->enabled = 1;
  return 0;
}

static int eth_tx_queue_setup(struct rte_eth_dev *dev,
                              uint16_t tx_queue_id,
                              uint16_t nb_tx_desc __rte_unused,
                              unsigned int socket_id __rte_unused,
                              const struct rte_eth_txconf *tx_conf __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  dev->data->tx_queues[tx_queue_id] = &internals->txq[tx_queue_id];
  internals->txq[tx_queue_id].enabled = 1;
  return 0;
}

static int _dev_set_mtu(struct rte_eth_dev *dev __rte_unused, uint16_t mtu)
{
  if (mtu < 46 || mtu > HW_MTU)
    return -EINVAL;

  return 0;
}

static int _dev_get_flow_ctrl(struct rte_eth_dev *dev __rte_unused, struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}

static int _dev_set_flow_ctrl(struct rte_eth_dev *dev __rte_unused, struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}

static const char *ActionErrorString(enum rte_flow_action_type type)
{
  switch (type) {
  case RTE_FLOW_ACTION_TYPE_PASSTHRU: return "Action PASSTHRU is not supported";
  case RTE_FLOW_ACTION_TYPE_MARK:     return "Action MARK is not supported";
  case RTE_FLOW_ACTION_TYPE_FLAG:     return "Action FLAG is not supported";
  case RTE_FLOW_ACTION_TYPE_DROP:     return "Action DROP is not supported";
  case RTE_FLOW_ACTION_TYPE_COUNT:    return "Action COUNT is not supported";
  case RTE_FLOW_ACTION_TYPE_DUP:      return "Action DUP is not supported";
  case RTE_FLOW_ACTION_TYPE_PF:       return "Action PF is not supported";
  case RTE_FLOW_ACTION_TYPE_VF:       return "Action VF is not supported";
  default:                            return "Action is UNKNOWN";
  }
}

static void _cleanUpHash(struct rte_flow *flow, struct pmd_internals *internals)
{
  struct rte_flow *pTmp;
  bool found = false;

  if (flow->rss_hf == 0) {
    // No hash => nothing to cleanup
    return;
  }
  LIST_FOREACH(pTmp, &internals->flows, next) {
    if (pTmp->rss_hf == flow->rss_hf && pTmp->port == flow->port && pTmp->priority == flow->priority) {
      // Key set is still in use
      found = true;
      break;
    }
  }
  if (!found) {
    // Hash is not in use anymore. delete it.
    DeleteHash(flow->rss_hf, flow->port, flow->priority, internals);
  }
}

static void _cleanUpKeySet(int key, struct pmd_internals *internals)
{
  struct rte_flow *pTmp;
  bool found = false;
  LIST_FOREACH(pTmp, &internals->flows, next) {
    if (pTmp->key == key) {
      // Key set is still in use
      found = true;
      break;
    }
  }
  if (!found) {
    // Key set is not in use anymore. delete it.
    DeleteKeyset(key, internals);
    RTE_LOG(DEBUG, PMD, "Returning keyset %u: %d\n", internals->adapterNo, key);
    ReturnKeysetValue(internals->adapterNo, key);
  }
}

static void _cleanUpFlow(struct rte_flow *flow, struct pmd_internals *internals)
{
  NtNtplInfo_t ntplInfo;
  char ntpl_buf[20];

  LIST_REMOVE(flow, next);
  while (!LIST_EMPTY(&flow->ntpl_id)) {
    struct filter_flow *id;
    id = LIST_FIRST(&flow->ntpl_id);
    sprintf(ntpl_buf, "delete=%d", id->ntpl_id);
    DoNtpl(ntpl_buf, &ntplInfo, internals);
    RTE_LOG(DEBUG, PMD, "Deleting Item filter: %s\n", ntpl_buf);
    LIST_REMOVE(id, next);
    free(id);
  }
  _cleanUpKeySet(flow->key, internals);
  _cleanUpHash(flow, internals);
  free(flow);
}

static struct rte_flow *_dev_flow_create(struct rte_eth_dev *dev,
                                  const struct rte_flow_attr *attr,
                                  const struct rte_flow_item items[],
                                  const struct rte_flow_action actions[],
                                  struct rte_flow_error *error)
{
  struct pmd_internals *internals = dev->data->dev_private;
  NtNtplInfo_t ntplInfo;
  uint8_t nb_queues = 0;
  uint8_t list_queues[RTE_ETHDEV_QUEUE_STAT_CNTRS];
  bool filterContinue = true;
  bool queueDefined = false;
  const struct rte_flow_action_rss *rss = NULL;
  const struct rte_flow_action_mark *mark = NULL;
  uint32_t i;
  bool tunnel;
  int version;
  int tunneltype;
  int color = -1;
  uint64_t typeMask = 0;
  bool reuse;

  char *ntpl_buf = NULL;
  struct rte_flow *flow = NULL;

  ntpl_buf = malloc(4096);
  if (!ntpl_buf) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }

  flow = malloc(sizeof(struct rte_flow));
  if (!flow) {
    rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Out of memory");
    goto FlowError;
  }
  memset(flow, 0, sizeof(struct rte_flow));

  if (attr->group) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL, "Attribute groups are not supported");
    goto FlowError;
  }
  if (attr->egress) {
    rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL, "Attribute egress is not supported");
    goto FlowError;
  }
  if (!attr->ingress) { rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, NULL, "Attribute ingress must be set");
  goto FlowError;
  }

  for (; actions->type != RTE_FLOW_ACTION_TYPE_END; ++actions) {
    switch (actions->type)
    {
    default:
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, ActionErrorString(actions->type));
      goto FlowError;
    case RTE_FLOW_ACTION_TYPE_VOID:
      continue;
    case RTE_FLOW_ACTION_TYPE_MARK:
      mark = (const struct rte_flow_action_mark *)actions->conf;
      if (COLOR_CHECK(mark->id)) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "mark id out of range. Must be less or equal to 0x1FFF");
        goto FlowError;
      }
      color = SET_COLOR(mark->id);
      break;
    case RTE_FLOW_ACTION_TYPE_RSS:
      rss = (const struct rte_flow_action_rss *)actions->conf;
      if (queueDefined) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Queue or RSS already defined");
        goto FlowError;
      }
      queueDefined = true;
      if (rss->num > RTE_ETHDEV_QUEUE_STAT_CNTRS) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Number of RSS queues out of range");
        goto FlowError;
      }
      for (i = 0; i < rss->num; i++) {
        if (rss->queue[i] >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "RSS queue out of range");
          goto FlowError;
        }
        list_queues[nb_queues++] = rss->queue[i];
      }
      break;
    case RTE_FLOW_ACTION_TYPE_QUEUE:
      if (queueDefined) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Queue or RSS already defined");
        goto FlowError;
      }
      queueDefined = true;
      if (((const struct rte_flow_action_queue *)actions->conf)->index >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "queue out of range");
        goto FlowError;
      }
      list_queues[nb_queues++] = ((const struct rte_flow_action_queue *)actions->conf)->index;
      break;
    }
  }

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

  // Set the priority
  sprintf(ntpl_buf, "assign[priority=%u;Descriptor=DYN2;", attr->priority);

  // Set the stream IDs
  CreateStreamid(&ntpl_buf[strlen(ntpl_buf)], internals, nb_queues, list_queues);

  // Set the port number
  sprintf(&ntpl_buf[strlen(ntpl_buf)], "]=port==%u", internals->port);


  // Set the filter expression
  tunnel = false;
  for (; items->type != RTE_FLOW_ITEM_TYPE_END; ++items) {
    switch (items->type) {
    case RTE_FLOW_ITEM_TYPE_VOID:
      continue;
    case RTE_FLOW_ITEM_TYPE_ETH:
      if (SetEthernetFilter(items,
                            tunnel,
                            &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up Ether filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_IPV4:
        if (SetIPV4Filter(&ntpl_buf[strlen(ntpl_buf)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up IPV4 filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_IPV6:
      if (SetIPV6Filter(&ntpl_buf[strlen(ntpl_buf)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up IPV6 filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_SCTP:
        if (SetSCTPFilter(&ntpl_buf[strlen(ntpl_buf)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up TCP filter");
          goto FlowError;
        }
        break;

      case RTE_FLOW_ITEM_TYPE_TCP:
        if (SetTCPFilter(&ntpl_buf[strlen(ntpl_buf)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up TCP filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_UDP:
      if (SetUDPFilter(&ntpl_buf[strlen(ntpl_buf)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up UDP filter");
        goto FlowError;
      }
      break;

      case RTE_FLOW_ITEM_TYPE_ICMP:
        if (SetICMPFilter(&ntpl_buf[strlen(ntpl_buf)],
                          &filterContinue,
                          items,
                          tunnel,
                          &typeMask) != 0) {
          rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up ICMP filter");
          goto FlowError;
        }
        break;

    case RTE_FLOW_ITEM_TYPE_VLAN:
      if (SetVlanFilter(&ntpl_buf[strlen(ntpl_buf)],
                        &filterContinue,
                        items,
                        tunnel,
                        &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up VLAN filter");
        goto FlowError;
      }
      break;

    case RTE_FLOW_ITEM_TYPE_GREv0:
    case RTE_FLOW_ITEM_TYPE_GREv1:
    case RTE_FLOW_ITEM_TYPE_GTPv1_U:
    case RTE_FLOW_ITEM_TYPE_GTPv0_U:
    case RTE_FLOW_ITEM_TYPE_VXLAN:
    case RTE_FLOW_ITEM_TYPE_NVGRE:
    case RTE_FLOW_ITEM_TYPE_IPinIP:
      switch (items->type) {
      case RTE_FLOW_ITEM_TYPE_GREv1:
        version = 1;
        tunneltype = GRE_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_GREv0:
        version = 0;
        tunneltype = GRE_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_GTPv1_U:
        version = 1;
        tunneltype = GTP_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_GTPv0_U:
        version = 0;
        tunneltype = GTP_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_VXLAN:
        version = 0;
        tunneltype = VXLAN_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_NVGRE:
        version = 0;
        tunneltype = NVGRE_TUNNEL_TYPE;
        break;
      case RTE_FLOW_ITEM_TYPE_IPinIP:
        version = 0;
        tunneltype = IP_IN_IP_TUNNEL_TYPE;
        break;
      default:
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up tunnel filter");
        goto FlowError;
      }

      if (SetTunnelFilter(&ntpl_buf[strlen(ntpl_buf)],
                        &filterContinue,
                        version,
                        tunneltype,
                        &typeMask) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Failed setting up tunnel filter");
        goto FlowError;
      }
      tunnel = true;
      break;

    default:
      rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Item is not supported");
      goto FlowError;

      }
    }

    if (CreateOptimizedFilter(ntpl_buf, internals, flow, &filterContinue, typeMask, list_queues, nb_queues, &reuse, color) != 0) {
      rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Creating filter error");
      goto FlowError;
    }

    if (!reuse) {
      if (DoNtpl(ntpl_buf, &ntplInfo, internals) != 0) {
        rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Filter error");
        goto FlowError;
      }
      pushNtplID(flow, ntplInfo.ntplId);
    }

    if (rss && rss->rss_conf) {
      // If RSS is used, then set the Hash mode
      if (CreateHash(rss->rss_conf->rss_hf, internals, flow, attr->priority + 1) != 0) {
        rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Failed setting up hash mode");
        goto FlowError;
      }
    }

    if (ntpl_buf) {
      free(ntpl_buf);
    }
    priv_lock(internals);
    LIST_INSERT_HEAD(&internals->flows, flow, next);
    priv_unlock(internals);
    return flow;

FlowError:
    if (flow) {
      free(flow);
    }
    if (ntpl_buf) {
      free(ntpl_buf);
    }
    return NULL;
}

static int _dev_flow_destroy(struct rte_eth_dev *dev,
                             struct rte_flow *flow,
                             struct rte_flow_error *error __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;

  priv_lock(internals);
  _cleanUpFlow(flow, internals);


  priv_unlock(internals);
  return 0;
}

static int _dev_flow_flush(struct rte_eth_dev *dev,
                           struct rte_flow_error *error __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;

  priv_lock(internals);
  while (!LIST_EMPTY(&internals->flows)) {
    struct rte_flow *flow;
    flow = LIST_FIRST(&internals->flows);
    _cleanUpFlow(flow, internals);
  }
  priv_unlock(internals);
  return 0;
}

static int _hash_filter_ctrl(struct rte_eth_dev *dev,
                             enum rte_filter_op filter_op,
                             void *arg)
{
  struct pmd_internals *internals = dev->data->dev_private;
  struct rte_eth_hash_filter_info *info = (struct rte_eth_hash_filter_info *)arg;
  int ret = 0;

  switch (filter_op) {
  case RTE_ETH_FILTER_NOP:
    break;
  case RTE_ETH_FILTER_SET:
    if (info->info_type == RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT) {
      if (info->info.enable) {
        if (internals->symHashMode != SYM_HASH_ENA_PER_PORT) {
          internals->symHashMode = SYM_HASH_ENA_PER_PORT;
        }
      }
      else {
        if (internals->symHashMode != SYM_HASH_DIS_PER_PORT) {
          internals->symHashMode = SYM_HASH_DIS_PER_PORT;
        }
      }
    }
    else {
      RTE_LOG(WARNING, PMD, ">>> Warning: Filter Hash - info_type (%d) not supported", info->info_type);
      ret = -ENOTSUP;
    }
    break;
  default:
    RTE_LOG(WARNING, PMD, ">>> Warning:  Filter Hash - Filter operation (%d) not supported", filter_op);
    ret = -ENOTSUP;
    break;
  }
  return ret;
}


static const struct rte_flow_ops _dev_flow_ops = {
  .validate = NULL,
  .create = _dev_flow_create,
  .destroy = _dev_flow_destroy,
  .flush = _dev_flow_flush,
  .query = NULL,
};

static int _dev_filter_ctrl(struct rte_eth_dev *dev __rte_unused,
                            enum rte_filter_type filter_type,
                            enum rte_filter_op filter_op,
                            void *arg)
{
  int ret = EINVAL;

  switch (filter_type) {
  case RTE_ETH_FILTER_HASH:
    ret = _hash_filter_ctrl(dev, filter_op, arg);
    return ret;
  case RTE_ETH_FILTER_GENERIC:
    switch (filter_op) {
    case RTE_ETH_FILTER_NOP:
      return 0;
    default:
      *(const void **)arg = &_dev_flow_ops;
      return 0;
    }
  default:
    RTE_LOG(ERR, PMD, "NTACC: %s: filter type (%d) not supported\n", __func__, filter_type);
    break;
  }

  return -ret;
}

static int eth_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
  char buf[51];
  struct pmd_internals *internals = dev->data->dev_private;

  snprintf(buf, 50, "%d.%d.%d - %03d-%04d-%02d-%02d-%02d", internals->version.major,
                                                           internals->version.minor,
                                                           internals->version.patch,
                                                           internals->fpgaid.s.item,
                                                           internals->fpgaid.s.product,
                                                           internals->fpgaid.s.ver,
                                                           internals->fpgaid.s.rev,
                                                           internals->fpgaid.s.build);
  size_t size = strlen(buf);
  strncpy(fw_version, buf, MIN(size+1, fw_size));
  return size;
}

static struct eth_dev_ops ops = {
    .dev_start = eth_dev_start,
    .dev_stop = eth_dev_stop,
    .dev_close = eth_dev_close,
    .mtu_set = _dev_set_mtu,
    .dev_configure = eth_dev_configure,
    .dev_infos_get = eth_dev_info,
    .rx_queue_setup = eth_rx_queue_setup,
    .tx_queue_setup = eth_tx_queue_setup,
    .rx_queue_release = eth_queue_release,
    .tx_queue_release = eth_queue_release,
    .link_update = eth_link_update,
    .stats_get = eth_stats_get,
    .stats_reset = eth_stats_reset,
    .flow_ctrl_get = _dev_get_flow_ctrl,
    .flow_ctrl_set = _dev_set_flow_ctrl,
    .filter_ctrl = _dev_filter_ctrl,
    .fw_version_get = eth_fw_version_get,
};

static int rte_pmd_init_internals(const char *name,
                                  const unsigned numa_node,
                                  const uint32_t port,
                                  const char     *ntpl_file,
                                  struct rte_eth_dev **eth_dev)
{
  int iRet = 0;
  struct pmd_internals *internals = NULL;
  struct rte_eth_dev_data *data = NULL;
  struct rte_device *device = NULL;
  uint i, status;
  char errBuf[NT_ERRBUF_SIZE];
  NtInfo_t info;
  struct rte_eth_link pmd_link;
#ifndef USE_SW_STAT
  NtStatistics_t statData;
#endif

  assert(port < MAX_NTACC_PORTS);

  RTE_LOG(DEBUG, PMD, "Creating ntacc-backend ethdev on numa socket %u\n", numa_node);

  /* now do all data allocation - for eth_dev structure, dummy pci driver
   * and internal (private) data
   */
  data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
  if (data == NULL) {
    RTE_LOG(ERR, PMD, "ERROR: Failed to allocate memory\n");
    iRet = 1;
    goto error;
  }

  device = rte_zmalloc_socket(name, sizeof(*device), 0, numa_node);
  if (device == NULL) {
    RTE_LOG(ERR, PMD, "ERROR: Failed to allocate memory\n");
    iRet = 1;
    goto error;
  }

  internals = rte_zmalloc_socket(name, sizeof(*internals), 0, numa_node);
  if (internals == NULL) {
    RTE_LOG(ERR, PMD, "ERROR: Failed to allocate memory\n");
    iRet = 1;
    goto error;
  }

  if (strlen(ntpl_file) > 0) {
    internals->ntpl_file  = rte_zmalloc_socket(name, strlen(ntpl_file) + 1, 0, numa_node);
    if (internals->ntpl_file == NULL) {
      RTE_LOG(ERR, PMD, "ERROR: Failed to allocate memory\n");
      iRet = 1;
      goto error;
    }
    strcpy(internals->ntpl_file, ntpl_file);
  }

  /* reserve an ethdev entry */
  *eth_dev = rte_eth_dev_allocate(name);
  if (*eth_dev == NULL) {
    RTE_LOG(ERR, PMD, "ERROR: Failed to allocate ether device\n");
    iRet = 1;
    goto error;
  }

  /* now put it all together
   * - store queue data in internals,
   * - store numa_node info in pci_driver
   * - point eth_dev_data to internals and pci_driver
   * - and point eth_dev structure to new eth_dev_data structure
   */
  /* NOTE: we'll replace the data element, of originally allocated eth_dev
   * so the rings are local per-process
   */

  /* Open the information stream */
  if ((status = (*_NT_InfoOpen)(&internals->hInfo, "DPDK Info stream")) != NT_SUCCESS) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, ">>> Error: NT_InfoOpen failed. Code 0x%x = %s\n", status, errBuf);
    iRet = status;
    goto error;
  }

  /* Find driver version */
  info.cmd = NT_INFO_CMD_READ_SYSTEM;
  if ((status = (*_NT_InfoRead)(internals->hInfo, &info)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_InfoRead failed. Code 0x%x = %s\n", status, errBuf);
    iRet = status;
    goto error;
  }

  internals->version.major = info.u.system.data.version.major;
  internals->version.minor = info.u.system.data.version.minor;
  internals->version.patch = info.u.system.data.version.patch;

  // Check that the driver is supported
  if (supportedDriver.major != internals->version.major ||
      supportedDriver.minor != internals->version.minor) {
    RTE_LOG(ERR, PMD, "ERROR: NT Driver version %d.%d.%d is not supported. The version must be %d.%d.%d.\n",
            internals->version.major, internals->version.minor, internals->version.patch,
            supportedDriver.major, supportedDriver.minor, supportedDriver.patch);
    iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
    goto error;
  }

  /* Find local port offset */
  info.cmd = NT_INFO_CMD_READ_PORT_V7;
  info.u.port_v7.portNo = (uint8_t)(port);
  if ((status = (*_NT_InfoRead)(internals->hInfo, &info)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_InfoRead failed. Code 0x%x = %s\n", status, errBuf);
    iRet = status;
    goto error;
  }

  internals->adapterNo = info.u.port_v7.data.adapterNo;
  internals->port = port;
  internals->local_port = port - info.u.port_v7.data.adapterInfo.portOffset;
  internals->featureLevel = info.u.port_v7.data.adapterInfo.featureLevel;
  internals->nbPorts = info.u.port_v7.data.adapterInfo.numPorts;
  internals->symHashMode = SYM_HASH_ENA_PER_PORT;
  internals->fpgaid.value = info.u.port_v7.data.adapterInfo.fpgaid.value;

  // Check if FPGA is supported
  for (i = 0; i < NB_SUPPORTED_FPGAS; i++) {
    if (supportedAdapters[i].item == internals->fpgaid.s.item &&
        supportedAdapters[i].product == internals->fpgaid.s.product) {
      if (supportedAdapters[i].ver != internals->fpgaid.s.ver ||
          supportedAdapters[i].rev != internals->fpgaid.s.rev) {
        RTE_LOG(ERR, PMD, "ERROR: NT adapter firmware %03d-%04d-%02d-%02d-%02d is not supported. The firmware must be %03d-%04d-%02d-%02d-%02d.\n",
            internals->fpgaid.s.item, internals->fpgaid.s.product, internals->fpgaid.s.ver, internals->fpgaid.s.rev, internals->fpgaid.s.build,
            supportedAdapters[i].item, supportedAdapters[i].product, supportedAdapters[i].ver, supportedAdapters[i].rev, supportedAdapters[i].build);
        iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
        goto error;
      }
      break;
    }
  }

  if (i == NB_SUPPORTED_FPGAS) {
    // No matching adapter is found
    RTE_LOG(ERR, PMD, "ERROR: No supported NT adapter is found. Following adapters are supported:\n");
    for (i = 0; i < NB_SUPPORTED_FPGAS; i++) {
      RTE_LOG(ERR, PMD, "       %03d-%04d-%02d-%02d-%02d\n",
         supportedAdapters[i].item, supportedAdapters[i].product, supportedAdapters[i].ver, supportedAdapters[i].rev, supportedAdapters[i].build);
    }
    iRet = NT_ERROR_NTPL_FILTER_UNSUPP_FPGA;
    goto error;
  }

  if (RTE_ETHDEV_QUEUE_STAT_CNTRS > STREAMIDS_PER_PORT) {
    RTE_LOG(ERR, PMD, ">>> Error: This adapter can only support %u queues\n", STREAMIDS_PER_PORT);
    RTE_LOG(ERR, PMD, "           Set RTE_ETHDEV_QUEUE_STAT_CNTRS to %u or less\n", STREAMIDS_PER_PORT);
    iRet = NT_ERROR_STREAMID_OUT_OF_RANGE;
    goto error;
  }

  for (i=0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internals->rxq[i].stream_id = STREAMIDS_PER_PORT * internals->local_port + i;
    internals->rxq[i].pSeg = NULL;
    internals->rxq[i].enabled = 0;
  }

  for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
    internals->txq[i].port = port;
    internals->txq[i].local_port = port - info.u.port_v7.data.adapterInfo.portOffset;
    internals->txq[i].enabled = 0;
  }

  switch (info.u.port_v7.data.speed) {
  case NT_LINK_SPEED_UNKNOWN:
    pmd_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10M:
    pmd_link.link_speed = ETH_SPEED_NUM_10M;
    break;
  case NT_LINK_SPEED_100M:
    pmd_link.link_speed = ETH_SPEED_NUM_100M;
    break;
  case NT_LINK_SPEED_1G:
    pmd_link.link_speed = ETH_SPEED_NUM_1G;
    break;
  case NT_LINK_SPEED_10G:
    pmd_link.link_speed = ETH_SPEED_NUM_10G;
    break;
  case NT_LINK_SPEED_40G:
    pmd_link.link_speed = ETH_SPEED_NUM_40G;
    break;
  case NT_LINK_SPEED_50G:
    pmd_link.link_speed = ETH_SPEED_NUM_50G;
    break;
  case NT_LINK_SPEED_100G:
    pmd_link.link_speed = ETH_SPEED_NUM_100G;
    break;
  }

  memcpy(&eth_addr[port].addr_bytes, &info.u.port_v7.data.macAddress, sizeof(eth_addr[port].addr_bytes));

  pmd_link.link_duplex = ETH_LINK_FULL_DUPLEX;
  pmd_link.link_status = 0;

  internals->if_index = internals->local_port;

  // Generate driver name
  strcpy(internals->driverName, "net_ntacc");

  device->numa_node = numa_node;

  data->dev_private = internals;
  data->port_id = (*eth_dev)->data->port_id;
  data->dev_link = pmd_link;
  data->mac_addrs = &eth_addr[port];
  data->numa_node = numa_node;
  data->drv_name = internals->driverName;
  strncpy(data->name, name, RTE_ETH_NAME_MAX_LEN);

  (*eth_dev)->data = data;
  (*eth_dev)->dev_ops = &ops;
  (*eth_dev)->device = device;

#ifndef USE_SW_STAT
  /* Open the stat stream */
  if ((status = (*_NT_StatOpen)(&internals->hStat, "DPDK Stat stream")) != NT_SUCCESS) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, ">>> Error: NT_StatOpen failed. Code 0x%x = %s\n", status, errBuf);
    iRet = status;
    goto error;
  }
  /* Reset stat data */
  statData.cmd = NT_STATISTICS_READ_CMD_QUERY_V2;
  statData.u.query_v2.poll=0;
  statData.u.query_v2.clear=1;
  if ((status = (*_NT_StatRead)(internals->hStat, &statData)) != 0) {
    (*_NT_ExplainError)(status, errBuf, sizeof(errBuf));
    RTE_LOG(ERR, PMD, "ERROR: NT_StatRead failed. Code 0x%x = %s\n", status, errBuf);
    iRet = status;
    goto error;
  }
#endif
  return iRet;

error:
  if (data)
    rte_free(data);
  if (device)
    rte_free(device);
  if (internals)
    rte_free(internals);
  return iRet;
}

/*
 * convert ascii to int
 */
static inline int ascii_to_u32(const char *key __rte_unused, const char *value, void *extra_args)
{
  *(uint32_t*)extra_args = atoi(value);
  return 0;
}

static inline int ascii_to_ascii(const char *key __rte_unused, const char *value, void *extra_args)
{
  strncpy((char *)extra_args, value, MAX_NTPL_NAME);
  return 0;
}

static int _nt_lib_open(void)
{
  char path[128];
  strcpy(path, NAPATECH3_LIB_PATH);
  strcat(path, "/libntapi.so");

  /* Load the library */
  _libnt = dlopen(path, RTLD_NOW);
  if (_libnt == NULL) {
    /* Library does not exist. */
    fprintf(stderr, "Failed to find needed library : %s\n", path);
    return -1;
  }
  _NT_Init = dlsym(_libnt, "NT_Init");
  if (_NT_Init == NULL) {
    fprintf(stderr, "Failed to find \"NT_Init\" in %s\n", path);
    return -1;
  }

  _NT_ConfigOpen = dlsym(_libnt, "NT_ConfigOpen");
  if (_NT_ConfigOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigOpen\" in %s\n", path);
    return -1;
  }
  _NT_ConfigClose = dlsym(_libnt, "NT_ConfigClose");
  if (_NT_ConfigClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_ConfigClose\" in %s\n", path);
    return -1;
  }
  _NT_NTPL = dlsym(_libnt, "NT_NTPL");
  if (_NT_NTPL == NULL) {
    fprintf(stderr, "Failed to find \"NT_NTPL\" in %s\n", path);
    return -1;
  }

  _NT_InfoOpen = dlsym(_libnt, "NT_InfoOpen");
  if (_NT_InfoOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoOpen\" in %s\n", path);
    return -1;
  }
  _NT_InfoRead = dlsym(_libnt, "NT_InfoRead");
  if (_NT_InfoRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoRead\" in %s\n", path);
    return -1;
  }
  _NT_InfoClose = dlsym(_libnt, "NT_InfoClose");
  if (_NT_InfoClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_InfoClose\" in %s\n", path);
    return -1;
  }
  _NT_StatClose = dlsym(_libnt, "NT_StatClose");
  if (_NT_StatClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatClose\" in %s\n", path);
    return -1;
  }
  _NT_StatOpen = dlsym(_libnt, "NT_StatOpen");
  if (_NT_StatOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatOpen\" in %s\n", path);
    return -1;
  }
  _NT_StatRead = dlsym(_libnt, "NT_StatRead");
  if (_NT_StatRead == NULL) {
    fprintf(stderr, "Failed to find \"NT_StatRead\" in %s\n", path);
    return -1;
  }
  _NT_ExplainError = dlsym(_libnt, "NT_ExplainError");
  if (_NT_ExplainError == NULL) {
    fprintf(stderr, "Failed to find \"NT_ExplainError\" in %s\n", path);
    return -1;
  }
  _NT_NetTxOpen = dlsym(_libnt, "NT_NetTxOpen");
  if (_NT_NetTxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetTxClose = dlsym(_libnt, "NT_NetTxClose");
  if (_NT_NetTxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpen = dlsym(_libnt, "NT_NetRxOpen");
  if (_NT_NetRxOpen == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpen\" in %s\n", path);
    return -1;
  }
  _NT_NetRxGet = dlsym(_libnt, "NT_NetRxGet");
  if (_NT_NetRxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGet\" in %s\n", path);
    return -1;
  }
  _NT_NetRxRelease = dlsym(_libnt, "NT_NetRxRelease");
  if (_NT_NetRxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxRelease\" in %s\n", path);
    return -1;
  }
  _NT_NetRxClose = dlsym(_libnt, "NT_NetRxClose");
  if (_NT_NetRxClose == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxClose\" in %s\n", path);
    return -1;
  }

  _NT_NetRxGetNextPacket = dlsym(_libnt, "NT_NetRxGetNextPacket");
  if (_NT_NetRxGetNextPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxGetNextPacket\" in %s\n", path);
    return -1;
  }

  _NT_NetRxOpenMulti = dlsym(_libnt, "NT_NetRxOpenMulti");
  if (_NT_NetRxOpenMulti == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetRxOpenMulti\" in %s\n", path);
    return -1;
  }

  _NT_NetTxRelease = dlsym(_libnt, "NT_NetTxRelease");
  if (_NT_NetTxRelease == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxRelease\" in %s\n", path);
    return -1;
  }

  _NT_NetTxAddPacket = dlsym(_libnt, "NT_NetTxAddPacket");
  if (_NT_NetTxAddPacket == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxAddPacket\" in %s\n", path);
    return -1;
  }

  _NT_NetTxGet = dlsym(_libnt, "NT_NetTxGet");
  if (_NT_NetTxGet == NULL) {
    fprintf(stderr, "Failed to find \"NT_NetTxGet\" in %s\n", path);
    return -1;
  }

  return 0;
}

static int rte_pmd_ntacc_dev_probe(const char *name, const char *params)
{
  unsigned numa_node;
  int ret = 0;
  struct rte_kvargs *kvlist;
  struct rte_eth_dev *eth_dev = NULL;
  NtNtplInfo_t ntplInfo;
  unsigned int i;
  uint32_t port=0;

  char ntplStr[MAX_NTPL_NAME] = { 0 };

  RTE_LOG(DEBUG, PMD, "Initializing pmd_ntacc %s for %s\n", rte_version(), name);

  numa_node = rte_socket_id();

  kvlist = rte_kvargs_parse(params, valid_arguments);
  if (kvlist == NULL) {
    return -1;
  }

  // Get port to use for Rx/Tx
  if ((i = rte_kvargs_count(kvlist, ETH_NTACC_PORT_ARG))) {
    assert (i == 1);
    ret = rte_kvargs_process(kvlist, ETH_NTACC_PORT_ARG, &ascii_to_u32, &port);
  }

  // Get filename to store ntpl
  if ((i = rte_kvargs_count(kvlist, ETH_NTACC_NTPL_ARG))) {
    assert (i == 1);
    ret = rte_kvargs_process(kvlist, ETH_NTACC_NTPL_ARG, &ascii_to_ascii, ntplStr);
  }

  rte_kvargs_free(kvlist);

  if (ret < 0)
    return -1;

  if (first) {
    ret = _nt_lib_open();
    if (ret < 0)
      return -1;
  }

  if (first)
    (*_NT_Init)(NTAPI_VERSION);

  if (rte_pmd_init_internals(name, numa_node, port, ntplStr, &eth_dev) < 0)
    return -1;

  if (first) {
    /* Delete all NTPL */
    struct pmd_internals *internals = eth_dev->data->dev_private;
    sprintf(ntplStr, "Delete=All");
    if (DoNtpl(ntplStr, &ntplInfo, internals) != 0) {
      return -1;
    }
  }

  eth_dev->rx_pkt_burst = eth_ntacc_rx;
  eth_dev->tx_pkt_burst = eth_ntacc_tx;

  first = 0;
  return 0;
}

static int rte_pmd_ntacc_dev_remove(const char *name)
{
  struct rte_eth_dev *eth_dev = NULL;

  RTE_LOG(DEBUG, PMD, "Closing ntacc ethdev on numa socket %u\n", rte_socket_id());

  if (name != NULL) {
    /* reserve an ethdev entry */
    eth_dev = rte_eth_dev_allocated(name);
    if (eth_dev != NULL) {
      struct pmd_internals *internals = eth_dev->data->dev_private;
      if (internals->ntpl_file) {
        rte_free(internals->ntpl_file);
      }
      rte_free(eth_dev->data->dev_private);
      rte_free(eth_dev->data);
      rte_eth_dev_release_port(eth_dev);
    }
  }

  if (_libnt != NULL)
    dlclose(_libnt);
  return 0;
}

static struct rte_vdev_driver pmd_ntacc_drv = {
  .probe = rte_pmd_ntacc_dev_probe,
  .remove = rte_pmd_ntacc_dev_remove,
};

RTE_PMD_REGISTER_VDEV(net_ntacc, pmd_ntacc_drv);
RTE_PMD_REGISTER_ALIAS(net_ntacc, eth_ntacc);
RTE_PMD_REGISTER_PARAM_STRING(net_ntacc,
  ETH_NTACC_NTPL_ARG     "=<string> "
  ETH_NTACC_PORT_ARG     "=<int> ");

