/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <unistd.h>

//#define CONTIGUOUS_MEMORY_BATCHING  // Define to use contiguous memory batching

#define RX_QUEUE_SIZE 128
#define TX_QUEUE_SIZE 512

#define NO_TX_QUEUES 4 // The number of TX queues per port (not used).

#define MAX_RX_PORTS  4 // The max number RX ports
#define MAX_RX_QUEUES 8

#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

volatile uint8_t quit_signal;

struct worker_data_s {
	uint8_t port;
	uint8_t queue;
	volatile uint64_t countPakets;
	volatile uint64_t countOctets;
	struct rte_mempool *mbuf_pool;
  uint8_t padding[32];
} __rte_cache_aligned;

struct worker_data_s workerData[MAX_RX_PORTS][MAX_RX_QUEUES];

static uint32_t number_of_ports = 1;
static uint32_t number_of_queues = 4;
#ifdef CONTIGUOUS_MEMORY_BATCHING
static uint32_t parse_type = 1;
#else
static uint32_t parse_type = 2;
#endif
static uint32_t useSwStat = 1;
static uint32_t dstIP[4] = {0};
static uint32_t srcIP[4] = {0};

static void
int_handler(int sig_num __rte_unused)
{
  quit_signal = 1;
}


/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf;
	struct rte_eth_rxconf rx_conf;
	int retval;
	uint16_t q;

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));
	port_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;

	///////////////////////////////////////////////////////
	// Select Contiguous Memory Batching for this queue. //
	///////////////////////////////////////////////////////
	memset(&rx_conf, 0,sizeof(struct rte_eth_rxconf));
#ifdef CONTIGUOUS_MEMORY_BATCHING
  if (parse_type != 2) {
    rx_conf.rxq_flags = ETH_RXQ_FLAGS_CMBATCH;  // Remove this line if contiguous memory batching
  }
#endif
																							// is not wanted for a queue

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, number_of_queues, NO_TX_QUEUES, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 4 RX queue per Ethernet port. */
	for (q = 0; q < number_of_queues; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_QUEUE_SIZE, rte_eth_dev_socket_id(port), &rx_conf, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 4 TX queue per Ethernet port. */
	for (q = 0; q < NO_TX_QUEUES; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_QUEUE_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n", (unsigned)port,
  																																																				addr.addr_bytes[0], addr.addr_bytes[1],
	  																																																			addr.addr_bytes[2], addr.addr_bytes[3],
		  																																																		addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

  return 0;
}

/*
 * The lcore_worker. This is the threads that does the work, reading from a port
 * and a queue.
 */
#ifdef CONTIGUOUS_MEMORY_BATCHING
static int lcore_worker_batch_parse(void *p)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	struct worker_data_s *data = (struct worker_data_s *)p;
	uint16_t nb_rx;

	// Check that we have a valid port.
	if (data->port > rte_eth_dev_count()) {
		printf("ERROR, Port %u is not present\n", data->port);
		return -1;
	}

	// Check that we have a valid queue
	if (data->queue >= number_of_queues) {
		printf("ERROR, Queue %u is not present\n", data->queue);
		return -1;
	}

	// Check that the port is on the same NUMA node as the polling thread for best performance.
	if (rte_eth_dev_socket_id(data->port) > 0 && rte_eth_dev_socket_id(data->port) != (int)rte_socket_id()) {
		printf("WARNING, port %u is on remote NUMA node to polling thread.\n\tPerformance will not be optimal.\n", data->port);
	}

	printf("Core %u numa %u capturing packets for port %d, Queue %d. "
         "Parsing done directly in batch buffer\n",
				  rte_lcore_id(), rte_lcore_to_socket_id(rte_lcore_id()), data->port, data->queue);

  data->countOctets = 0;
  data->countPakets = 0;

	/* Run until the application is quit or killed. */
	for (;;) {
		if (quit_signal) break;
		/* Get burst of RX packets */
		nb_rx = rte_eth_rx_burst(data->port, data->queue, bufs, BURST_SIZE);

		if (likely(nb_rx)) {
			/* Count captured packets and measure throughput... */
			uint32_t buf;
			uint32_t pack;

			for (buf = 0; buf < nb_rx; buf++) {
				struct rte_mbuf *mbuf = bufs[buf];
        /////////////////////////////////////////////////////////////////
        //
        // PKT_BATCH is set. The mbuf hold a batch of packets
        //
        // mbuf->buf_addr: Pointer to the batch buffer
        //
        // mbuf->batch_nb_packet: The number of packets in a batch.
        //
        // mbuf->ol_flags: (PKT_BATCH | CTRL_MBUF_FLAG) indicating that the
        //                 mbuf contains a batch of packets
        //
        // mbuf->data_off: Not used. Always 0
        //
        // mbuf->data_len: Length of the first packet in the batch buffer
        //
        // mbuf->pkt_len: Length of the batch buffer
        //
        // mbuf->userdata: Pointer to the batch control buffer
        //                 Must not be changed
        //
        // mbuf->port: DPDK port number.
        //
        // mbuf->batch_release_cb: Pointer to callback function called
        //                         when the batch buffer is released
        /////////////////////////////////////////////////////////////////

				if (likely(mbuf->ol_flags & PKT_BATCH)) {
					/////////////////////////////////////////////////////////////////
					// Browse batch of packets directly in the batch buffer
					//
					// Note: This batch of packets has to be released before a
					//       new batch of packets is requested.
					//       If a packet must be kept for later analysis,
					//       it must be copied to a new mbuf with the
					//       function: rte_pktmbuf_cmbatch_copy_packet_from_batch
					/////////////////////////////////////////////////////////////////

					struct rte_mbuf_batch_pkt_hdr *phdr;

					phdr = mbuf->buf_addr;  // Point to the beginning of the batch buffer
					for (pack = 0; pack < mbuf->batch_nb_packet; pack++) {
	#if 1
						/////////////////////////////////////////////////////////////////
						// Just count the data.
						/////////////////////////////////////////////////////////////////

            data->countOctets += phdr->wireLength;
            data->countPakets++;
	#else
					/////////////////////////////////////////////////////////////////
					// Copy the packet to a new mbuf for later use.
					/////////////////////////////////////////////////////////////////

						struct rte_mbuf *next;
						struct rte_mbuf *m = rte_pktmbuf_cmbatch_copy_packet_from_batch(phdr, data->mbuf_pool);
						next = m;
						while (next != NULL) {
							data->countOctets += next->data_len; // The wire length
							next = next->next;
						}
						data->countPakets++;
						rte_pktmbuf_free(m);
	#endif

						/////////////////////////////////////////////////////////////////
						//
						// The following values are read from the packet descriptor.
						//
						// mbuf->buf_addr: Pointer to the beginning of the batch buffer
						//
						// phdr->storedLength: The length of the packet including the descriptor
						//
						// phdr->wireLength: The length of the packet on the wire
						//
						// phdr->rxPort: The port number, the packet is received on.
						//               Note: The port number is the local port number of the adapter
						//                     It is not the DPDK port number.
						//
						// phdr->descrLength: The length of the descriptor
						//
						// phdr->timestamp: Packet timestamp
						//
						// phdr->offset0: Offset to the layer3 header.
						//                Note: Can be changed in common_base
						//
						// phdr->color_hi: Packet hash value. if (phdr->descrLength == 20)
						//
						// ((phdr->color_hi << 14) & 0xFFFFC000) | phdr->color_lo:
						//              Packet MARK value. if (phdr->descrLength == 22)
						//
						/////////////////////////////////////////////////////////////////

	#if 0 // Dump IPV4 header
						{
							#define IPV4_ADDRESS(a) ((const char *)&a)[0] & 0xFF, \
																			((const char *)&a)[1] & 0xFF, \
																			((const char *)&a)[2] & 0xFF, \
																			((const char *)&a)[3] & 0xFF

							// Point to Layer3 using the data offset
							struct ipv4_hdr *ipv4hdr = (struct ipv4_hdr *)((uint8_t *)phdr + phdr->descrLength + phdr->offset0);

							printf("Src IP: %u.%u.%u.%u - Dst IP: %u.%u.%u.%u\n",
										 IPV4_ADDRESS(ipv4hdr->src_addr),
										 IPV4_ADDRESS(ipv4hdr->dst_addr));
						}
	#endif  // Dump IPV4 header

						// Point to next packet
						phdr = (struct rte_mbuf_batch_pkt_hdr *)((u_char *)phdr + phdr->storedLength);
					}
				}
				else {
          printf("ERROR: Non batch mbuf %u received on port %u\n", data->queue, data->port);
					quit_signal = 1;
					return 0;
			  }
        rte_pktmbuf_free(bufs[buf]);
		  }
	  }
  }
	return 0;
}
#endif

/*
 * The lcore_worker. This is the threads that does the work, reading from a port
 * and a queue.
 */
#ifdef CONTIGUOUS_MEMORY_BATCHING
static int lcore_worker_parse_helper(void *p)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	struct worker_data_s *data = (struct worker_data_s *)p;
	uint16_t nb_rx;

	// Check that we have a valid port.
	if (data->port > rte_eth_dev_count()) {
		printf("ERROR, Port %u is not present\n", data->port);
		return -1;
	}

	// Check that we have a valid queue
	if (data->queue >= number_of_queues) {
		printf("ERROR, Queue %u is not present\n", data->queue);
		return -1;
	}

	// Check that the port is on the same NUMA node as the polling thread for best performance.
	if (rte_eth_dev_socket_id(data->port) > 0 && rte_eth_dev_socket_id(data->port) != (int)rte_socket_id()) {
		printf("WARNING, port %u is on remote NUMA node to polling thread.\n\tPerformance will not be optimal.\n", data->port);
	}

	printf("Core %u numa %u capturing packets for port %d, Queue %d. "
         "Parsing done using helper functions.\n",
				 rte_lcore_id(), rte_lcore_to_socket_id(rte_lcore_id()), data->port, data->queue);

	data->countOctets = 0;
	data->countPakets = 0;

	/* Run until the application is quit or killed. */
	for (;;) {
		if (quit_signal) break;
		/* Get burst of RX packets */
		nb_rx = rte_eth_rx_burst(data->port, data->queue, bufs, BURST_SIZE);

		if (likely(nb_rx)) {
			/* Count captured packets and measure throughput... */
			uint32_t buf;
			uint32_t pack;

			for (buf = 0; buf < nb_rx; buf++) {
				struct rte_mbuf *mbuf = bufs[buf];
				if (likely(mbuf->ol_flags & PKT_BATCH)) {
					/////////////////////////////////////////////////////////////////
					//
					// PKT_BATCH is set. The mbuf hold a batch of packets
					//
					// mbuf->buf_addr: Pointer to the batch buffer
					//
					// mbuf->batch_nb_packet: The number of packets in a batch.
					//
					// mbuf->ol_flags: (PKT_BATCH | CTRL_MBUF_FLAG) indicating that the
					//                 mbuf contains a batch of packets
					//
					// mbuf->data_off: Not used. Always 0
					//
					// mbuf->data_len: Length of the first packet in the batch buffer
					//
					// mbuf->pkt_len: Length of the batch buffer
					//
					// mbuf->userdata: Pointer to the batch control buffer
					//                 Must not be changed
					//
					// mbuf->port: DPDK port number.
					//
					// mbuf->batch_release_cb: Pointer to callback function called
					//                         when the batch buffer is released
					/////////////////////////////////////////////////////////////////

					/////////////////////////////////////////////////////////////////
					// Using inline function to browse batch of packets
					//
					// Note: This batch of packets has to be released before a
					//       new batch of packets is requested.
					//       If a packet must be kept for later analysis,
					//       it must be copied to a new mbuf with the
					//       function: rte_pktmbuf_cmbatch_copy_packet_from_mbuf
					/////////////////////////////////////////////////////////////////

					uint32_t offset;      // Offset in batch buffer
					struct rte_mbuf m1;    // mbuf act as pointer to a single packet.

					offset = 0; // Set to 0 to indicate the first packet
					for (pack = 0; pack < mbuf->batch_nb_packet; pack++) {
						rte_pktmbuf_cmbatch_get_next_packet(mbuf, &m1, &offset); // Copy pointers and info to the mbuf.
	#if 1
					/////////////////////////////////////////////////////////////////
					// Just count the data.
					/////////////////////////////////////////////////////////////////

						data->countOctets += m1.data_len;   // This is the wirelength
						data->countPakets++;
	#else
						/////////////////////////////////////////////////////////////////
						// Copy the packet to a new mbuf for later use.
						/////////////////////////////////////////////////////////////////

						struct rte_mbuf *next;
						struct rte_mbuf *m2 = rte_pktmbuf_cmbatch_copy_packet_from_mbuf(&m1, data->mbuf_pool);
						next = m2;
						while (next != NULL) {
							data->countOctets += next->data_len; // The wire length
							next = next->next;
						}
						data->countPakets++;
						rte_pktmbuf_free(m2);
	#endif

						/////////////////////////////////////////////////////////////////
						//
						// The following values are copied to the local mbuf by the
						// rte_pktmbuf_cmbatch_get_next_packet inline function
						//
						// m.buf_addr: Pointer to the packet including the packet descriptor
						//
						// m.port:     The port number, the packet is received on.
						//             Note: The port number is the local port number of the adapter
						//                   It is not the DPDK port number.
						//
						// m.data_len: The length of the packet on the wire
						//
						// m.pkt_len:  The captured length of the packet
						//
						// m.data_off: Offset to the layer2 header. After the descriptor.
						//
						// m.hash.rss: HASH value of the packet. if (m->ol_flags & PKT_RX_RSS_HASH)
						//
						// m.hash.fdir.hi: The MARK value. if (m->ol_flags & (PKT_RX_FDIR_ID | PKT_RX_FDIR))
						//
						// m.timestamp: Packet timestamp. if (m->ol_flags & PKT_RX_TIMESTAMP)
						//
						/////////////////////////////////////////////////////////////////

	#if 0 //  Dump IPV4 header
					  {
						  #define IPV4_ADDRESS(a) ((const char *)&a)[0] & 0xFF, \
									  									((const char *)&a)[1] & 0xFF, \
								  										((const char *)&a)[2] & 0xFF, \
							  											((const char *)&a)[3] & 0xFF

						  // Point to Layer3 using the data offset
						  struct ipv4_hdr *ipv4hdr = (struct ipv4_hdr *)((uint8_t *)m.buf_addr + m.data_off + ETHER_HDR_LEN);

						  printf("Src IP: %u.%u.%u.%u - Dst IP: %u.%u.%u.%u\n",
							  		 IPV4_ADDRESS(ipv4hdr->src_addr),
						  			 IPV4_ADDRESS(ipv4hdr->dst_addr));
					  }
	#endif  // Dump IPV4 header
				  }
			  }
			  else {
          printf("ERROR: Non batch mbuf %u received on port %u\n", data->queue, data->port);
  			  quit_signal = 1;
				  return 0;
			  }
        rte_pktmbuf_free(bufs[buf]);
		  }
	  }
  }
	return 0;
}
#endif


/*
 * The lcore_worker. This is the threads that does the work, reading from a port
 * and a queue.
 */
static int lcore_worker_mbuf(void *p)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	struct worker_data_s *data = (struct worker_data_s *)p;
	uint16_t nb_rx;

	// Check that we have a valid port.
	if (data->port > rte_eth_dev_count()) {
		printf("ERROR, Port %u is not present\n", data->port);
		return -1;
	}

	// Check that we have a valid queue
	if (data->queue >= number_of_queues) {
		printf("ERROR, Queue %u is not present\n", data->queue);
		return -1;
	}

	// Check that the port is on the same NUMA node as the polling thread for best performance.
	if (rte_eth_dev_socket_id(data->port) > 0 && rte_eth_dev_socket_id(data->port) != (int)rte_socket_id()) {
		printf("WARNING, port %u is on remote NUMA node to polling thread.\n\tPerformance will not be optimal.\n", data->port);
	}

	printf("Core %u numa %u capturing packets for port %d, Queue %d. Using normal mbufs\n",
				 rte_lcore_id(), rte_lcore_to_socket_id(rte_lcore_id()), data->port, data->queue);

	data->countOctets = 0;
	data->countPakets = 0;

	/* Run until the application is quit or killed. */
	for (;;) {
		if (quit_signal) break;
		/* Get burst of RX packets */
		nb_rx = rte_eth_rx_burst(data->port, data->queue, bufs, BURST_SIZE);

		if (likely(nb_rx)) {
			/* Count captured packets and measure throughput... */
			uint32_t buf;

			for (buf = 0; buf < nb_rx; buf++) {
				struct rte_mbuf *mbuf = bufs[buf];
				/////////////////////////////////////////////////////////////////
				/////////////////////////////////////////////////////////////////
				// PKT_BATCH is NOT set. Packet based RX (one packets per mbuf
				/////////////////////////////////////////////////////////////////
				/////////////////////////////////////////////////////////////////

#if 0 // Dump IPV4 header
				{
					#define IPV4_ADDRESS(a) ((const char *)&a)[0] & 0xFF, \
					                        ((const char *)&a)[1] & 0xFF, \
																  ((const char *)&a)[2] & 0xFF, \
																	((const char *)&a)[3] & 0xFF

					// Point to Layer3 using the data offset
					struct ipv4_hdr *ipv4hdr = (struct ipv4_hdr *)((uint8_t *)mbuf->buf_addr + mbuf->data_off);

					printf("Src IP: %u.%u.%u.%u - Dst IP: %u.%u.%u.%u\n",
								 IPV4_ADDRESS(ipv4hdr->src_addr),
								 IPV4_ADDRESS(ipv4hdr->dst_addr));
				}
#endif  // Dump IPV4 header

				while (mbuf != NULL) {
				  data->countOctets += mbuf->data_len; // The wire length
					mbuf = mbuf->next;
				}
				data->countPakets++;
				rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
	return 0;
}

/*
 * The lcore_main This is the thread that collects and prints statistics
 */

static void lcore_main(void)
{
	struct rte_eth_stats stats;
	uint64_t tsc1, tsc2;
	uint64_t hz = rte_get_timer_hz();

  struct {
    uint64_t lastOctets;
    uint64_t calcOctets;
    uint64_t totalPackets;
    uint64_t totalOctets;
  } stat[MAX_RX_PORTS][MAX_RX_QUEUES];

	uint i;
	uint j;

	sleep(1);
	printf("Core %u numa %u is handling %s statistics output\n",
         rte_lcore_id(), rte_lcore_to_socket_id(rte_lcore_id()), useSwStat == 0?"hardware":"software");
	for (j = 0; j < number_of_ports; j++) {
		for (i = 0; i < number_of_queues; i++) {
      stat[j][i].lastOctets = 0;
		}
		if (useSwStat == 0) {
			rte_eth_stats_reset(j);
		}
	}
	printf("\n");

	tsc1 = rte_rdtsc();
	while (1) {
		sleep(1);
		if (quit_signal) break;
		tsc2 = rte_rdtsc();

    if (useSwStat == 1) {
			for (j = 0; j < number_of_ports; j++) {
				for (i = 0; i < number_of_queues; i++) {
					// Sample number of received packets
          stat[j][i].totalPackets = workerData[j][i].countPakets;
					// Sample number of received bytes
					stat[j][i].totalOctets = workerData[j][i].countOctets;
				}
			}

			for (j = 0; j < number_of_ports; j++) {
				for (i = 0; i < number_of_queues; i++) {
					// Calculate the number of received bytes since last update.
					stat[j][i].calcOctets = stat[j][i].totalOctets - stat[j][i].lastOctets;
					stat[j][i].lastOctets = stat[j][i].totalOctets;
				}
			}
    }
		else {
			for (j = 0; j < number_of_ports; j++) {
				// Query the stat for a given port.
				rte_eth_stats_get(j, &stats);
				for (i = 0; i < number_of_queues; i++) {
					stat[j][i].calcOctets = stats.q_ibytes[i] - stat[j][i].lastOctets;
					stat[j][i].lastOctets = stats.q_ibytes[i];
					stat[j][i].totalPackets = stats.q_ipackets[i];
				}
			}
		}

		double div = (double)(tsc2-tsc1)/(double)hz;
		for (j = 0; j < number_of_ports; j++) {
			for (i = 0; i < number_of_queues; i++) {
				printf("Q%u,%u: %llu pk, %7.1f Mbps ", j, i, (long long unsigned)stat[j][i].totalPackets, ((double)stat[j][i].calcOctets*8/((double)1000000)/div));
			}
		}
		printf("\r");
		fflush(stdout);
		tsc1 = tsc2;
	}
	printf("\n\n");
	for (j = 0; j < number_of_ports; j++) {
		for (i = 0; i < number_of_queues; i++) {
			printf("Port %u - Queue %u: Received %16llu pkts\n", j, i, (long long unsigned)stat[j][i].totalPackets);
		}
	}
}

/*
 * Setup a rte flow filter
 */
static int SetupFilter(uint8_t portid, struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[100];
	struct rte_flow_action actions[10];
	struct rte_eth_hash_filter_info info;
	struct rte_flow *flow;
	uint32_t actionCount = 0;
	uint32_t patternCount = 0;
	uint i;

	// Pattern struct
	struct rte_flow_item_ipv4 ipv4_spec;
	memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));

	// Use a receive side scaling
	struct rte_flow_action_rss *rss = rte_zmalloc(
		"cmbatch",
		sizeof(struct rte_flow_action_rss) + sizeof(uint16_t) * number_of_queues,
		0);

  if (!rss) {
		printf("Memory allocation failure\n");
		return -1;
  }

	struct rte_eth_rss_conf rss_conf;

	// Use a single queue
	struct rte_flow_action_queue queue;

	struct rte_flow_action_mark mark;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(error, 0x22, sizeof(struct rte_flow_error));

	memset(&attr, 0, sizeof(attr));
	attr.ingress = 1;
	attr.priority = 1;

	memset(&pattern, 0, sizeof(pattern));

	// Recieve all IPV4 trafic.
  if (dstIP[0] != 0) {
		ipv4_spec.hdr.dst_addr = rte_cpu_to_be_32(IPv4(dstIP[0], dstIP[1], dstIP[2], dstIP[3] + portid));
  }
	if (srcIP[0] != 0) {
		ipv4_spec.hdr.src_addr = rte_cpu_to_be_32(IPv4(srcIP[0], srcIP[1], srcIP[2], srcIP[3] + portid));
	}
	pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[patternCount].spec = &ipv4_spec;
	patternCount++;

	pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
	patternCount++;

	memset(&actions, 0, sizeof(actions));

	if (number_of_queues > 1) {
		// Use receive side scaling
		rss_conf.rss_key = NULL;
		rss_conf.rss_key_len = 0;
		rss_conf.rss_hf = ETH_RSS_IPV4;

      rss->num = number_of_queues;
		for (i = 0; i < number_of_queues; i++) {
        rss->queue[i] = i;
		}
		rss->rss_conf = &rss_conf;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
		actions[actionCount].conf = rss;
		actionCount++;
	}
	else {
		// Use a single queue
		queue.index = 0;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		actions[actionCount].conf = &queue;
		actionCount++;
	}

	// Set MARK
	mark.id = portid * 0x100 + 0xFF;
	actions[actionCount].type = RTE_FLOW_ACTION_TYPE_MARK;
	actions[actionCount].conf = &mark;
	actionCount++;

	actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
	actionCount++;

	// Set symmetric hash
	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
	info.info.enable = 1;
	if (rte_eth_dev_filter_ctrl(portid, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info) < 0) {
	 	printf("Cannot set symmetric hash enable per port on port %u\n", portid);
		rte_free(rss);
	 	return -1;
	}

	// Delete the default filter
	if (rte_flow_isolate(portid, 1, error) < 0) {
	 	printf("Isolate failed on port %u\n", portid);
		rte_free(rss);
	 	return -1;
	}

	// Create the ret flow filter
	flow = rte_flow_create(portid, &attr, pattern, actions, error);
	if (flow == NULL) {
		rte_free(rss);
		return -1;
	}
	rte_free(rss);
  return 0;
}

static const char short_options[] =
	"p:"  /* Number of port to use            */
	"q:"  /* number of queues per port to use */
	"t:"  /* Type of parsing done */
	"s:"  /* Type of statistic used */
	"d:"  /* Destination IP address to use in filter */
  "i:"  /* Source IP address to use in filter */
	;

/* display usage */
static void
cmbatch_usage(const char *prgname)
{
	printf("\n%s [EAL options] -- [-p no_ports][-q queues_per_port][-t parse_type]"
				 "[-s stat_type][-i ip_addr][-d ip_addr][-e]\n"
	       "  -p no_ports: Number of ports to use. Always starting with port 0.\n"
	       "  -q queues_per_port: Number of queue per port \n"
				 "  -t parse_type: Type of parsing done \n"
#ifdef CONTIGUOUS_MEMORY_BATCHING
				 "                 0: Parse packets directly from the batch buffer\n"
				 "                 1: Parse packets using helper function\n"
#endif
         "                 2: Normal mbuf\n"
				 "  -s stat_type:  Type of statistic used\n"
				 "                 0: Use hardware statistics\n"
				 "                 1: Use software statistics\n"
				 "  -i ip_addr:    Source IP address to use in filter\n"
				 "  -d ip_addr:    Destination IP address to use in filter\n"
				 "\n"
				 "  lcores used are equal to no_ports * queues_per_port + 1\n\n",
	       prgname);
}

static unsigned int
cmbatch_parse_value(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
cmbatch_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt(argc, argvopt, short_options)) != EOF) {
		switch (opt) {
		case 'p':
			number_of_ports = cmbatch_parse_value(optarg);
			if (number_of_ports == 0 || number_of_ports > MAX_RX_PORTS) {
				printf("Invalid number of ports - Must be between 0 and %u\n", MAX_RX_PORTS);
				cmbatch_usage(prgname);
				return -1;
			}
			break;

		case 'q':
			number_of_queues = cmbatch_parse_value(optarg);
			if (number_of_queues == 0 || number_of_queues > MAX_RX_QUEUES) {
				printf("Invalid number of queues - Must be between 0 and %u\n", MAX_RX_QUEUES);
				cmbatch_usage(prgname);
				return -1;
			}
			break;

		case 't':
			parse_type = cmbatch_parse_value(optarg);
#ifdef CONTIGUOUS_MEMORY_BATCHING
			if (parse_type != 0 && parse_type != 1 && parse_type != 2) {
#else
        if (parse_type != 2) {
#endif
				printf("Invalid parse type selected\n");
				cmbatch_usage(prgname);
				return -1;
			}
			break;

		case 's':
			useSwStat = cmbatch_parse_value(optarg);
			if (useSwStat != 0 && useSwStat != 1) {
				printf("Invalid statistics type selected\n");
				cmbatch_usage(prgname);
				return -1;
			}
			break;

		case 'd':
			{
				sscanf(optarg, "%d.%d.%d.%d", &dstIP[0], &dstIP[1], &dstIP[2], &dstIP[3]);
			}
			break;

		case 'i':
			{
				sscanf(optarg, "%d.%d.%d.%d", &srcIP[0], &srcIP[1], &srcIP[2], &srcIP[3]);
			}
			break;

		default:
			cmbatch_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_flow_error error;
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint8_t portid;
	uint8_t queue;
	unsigned lcore_id;

	quit_signal = 0;

	/* catch ctrl-c so we can print on exit */
	signal(SIGINT, int_handler);
	signal(SIGTERM, int_handler);

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
  }

	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = cmbatch_parse_args(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid cmbatch arguments\n");
  }

  /* Check that there is at least one port. */
	nb_ports = rte_eth_dev_count();
	if (nb_ports < 1) {
		rte_exit(EXIT_FAILURE, "No ports found\n");
  }

	/* Check that the number of specified port is valid. */
	if (number_of_ports > nb_ports) {
		rte_exit(EXIT_FAILURE, "To many ports specified %u. Only %u present\n", number_of_ports, nb_ports);
	}

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * number_of_ports, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
  }

	/* Initialize all ports. */
	for (portid = 0; portid < number_of_ports; portid++) {
		if (port_init(portid, mbuf_pool) != 0) {
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
    }
	}

	// Check that there are the right number of lcores.
	if (rte_lcore_count() != ((number_of_queues * number_of_ports) + 1)) {
		rte_exit(EXIT_FAILURE, "Need %u cores specified", (number_of_queues * number_of_ports) + 1);
	}

	// Setup a rte flow filter for each port.
	for (portid = 0; portid < number_of_ports; portid++) {
		if (SetupFilter(portid, &error) != 0) {
			rte_exit(EXIT_FAILURE, "Error setting up filter: %s\n", error.message);
		}
	}

  /* launch worker thread for receiving data. */
	portid = 0;
	queue = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
    workerData[portid][queue].port = portid;
    workerData[portid][queue].queue = queue;
    workerData[portid][queue].mbuf_pool = mbuf_pool;
#ifdef CONTIGUOUS_MEMORY_BATCHING
    if (parse_type == 0) {
      rte_eal_remote_launch((lcore_function_t *)lcore_worker_parse_helper, (void *)&workerData[portid][queue], lcore_id);
    }
    else if (parse_type == 1) {
      rte_eal_remote_launch((lcore_function_t *)lcore_worker_batch_parse, (void *)&workerData[portid][queue], lcore_id);
    }
    else
#endif
    if (parse_type == 2) {
      rte_eal_remote_launch((lcore_function_t *)lcore_worker_mbuf, (void *)&workerData[portid][queue], lcore_id);
    }
    else {
      rte_exit(EXIT_FAILURE, "Illegal parse type\n");
    }
		queue++;
		if (queue >= number_of_queues) {
			portid++;
			queue = 0;
		}
	}

	/* Call lcore_main on the master core only. */
	lcore_main();

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
		  return -1;
    }
	}

	/* Stop and close down ports */
	for (portid = 0; portid < nb_ports; portid++) {
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
	}
	return 0;
}
