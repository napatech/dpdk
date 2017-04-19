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

#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "flow_classification.h"

/* Number of bytes needed for each mbuf */
#define MBUF_SIZE \
	(ETHER_MAX_LEN + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
/* Number of mbufs in mempool that is created */
#define NUM_MBUFS                     16384
/* How many packets to attempt to read from NIC in one go */
#define BURST_SIZE                      32 /* Cannot be larger than 64. 32 seems to be standard */
/* How many objects (mbufs) to keep in per-lcore mempool cache */
#define MEMPOOL_CACHE_SIZE      			  256 /* Must be larger than burst size. 256 seems to be standard */
/* Number of RX ring descriptors */
#define NUM_RXD                        2048
/* Number of TX ring descriptors */
#define NUM_TXD                        2048
/* Max number of flows to support. Must be power2 */
#define MAX_NUM_FLOWS 1<<20 // ~1E6

/* Mempool for mbufs */
static struct rte_mempool * pktmbuf_pool = NULL;

/* Global variables used for statistics */
double handlingpkts_tsc, total_tsc;
unsigned long long sw_drop;

struct sw_stats {
	uint64_t ibytes;
	uint64_t ipackets;
} sw_stats_up[2], sw_stats_down[2];

volatile int running = 1;
#ifdef SHOW_FLOW_IN_HW_COUNT
extern volatile uint32_t flows_cur_in_hw;
#endif
/* Print out statistics on packets handled */
static void
print_stats(void)
{
#define DELTA_PS(A, F, TS) (double)(A[1].F - A[0].F)/TS
	static struct timespec ts[2];

	if (ts[0].tv_sec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &ts[0]);
	}
  clock_gettime(CLOCK_MONOTONIC, &ts[1]);

	const uint64_t mdt = ((ts[1].tv_sec*1000000+ts[1].tv_nsec/1000) -
	                     (ts[0].tv_sec*1000000+ts[0].tv_nsec/1000));


	printf("Upstream: %9.0f kpps | %9.3f Mbps - "
	       "Downstream: %9.0f kpps | %9.3f Mbps - "
	       "Drops %lld - "
	       "CPU util %2.0f%%",
	         DELTA_PS(sw_stats_up, ipackets, mdt)*1000,
	         DELTA_PS(sw_stats_up, ibytes, mdt)*8,
	         DELTA_PS(sw_stats_down, ipackets, mdt)*1000,
	         DELTA_PS(sw_stats_down, ibytes, mdt)*8,
	         sw_drop,
	         ((handlingpkts_tsc*100)/total_tsc));
#ifdef SHOW_FLOW_IN_HW_COUNT
	printf("\tIn HW: %d", flows_cur_in_hw);
#endif
	printf("       \r");
	fflush(stdout);
	handlingpkts_tsc=0;
	total_tsc=0;
	ts[0] = ts[1];
	sw_stats_up[0] = sw_stats_up[1];
	sw_stats_down[0] = sw_stats_down[1];
}

/* Custom handling of signals to handle stats */
static void
signal_handler(int signum)
{
	switch(signum) {
		case SIGALRM:
		  print_stats();
  		alarm(1); // Rearm
			break;
		case SIGINT:
			printf("Terminating...\n");
			running = 0;
			break;
	}
}


/* Number of ports */
#define NUM_PORTS                        2 // Uplink + Downlink
#define UPLINK                           0
#define Downlink                         1

/* Main processing loop */
static int
main_loop(__attribute__((unused)) void *arg)
{
  fflush(stdout);
  uint64_t cur_tsc, prev_tsc;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	prev_tsc = (ts.tv_sec*1000000000+ts.tv_nsec);
	void *handle;
	if (flow_classification_init(&handle, MAX_NUM_FLOWS, rte_lcore_id()) != 0) {
		return 1;
	}

	alarm(1);

  while(running) {
    unsigned b;
    b=0;
		clock_gettime(CLOCK_MONOTONIC, &ts);
    cur_tsc = (ts.tv_sec*1000000000+ts.tv_nsec);
		for (uint8_t port = 0; port < NUM_PORTS; port++) {
			/* Get burst of RX packets, from first port of pair. */
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);

			if (nb_rx) {
				b=1;
			}

			uint8_t nb_out = 0;
			struct rte_mbuf *out[BURST_SIZE];

			flow_classification(handle, bufs, nb_rx, out, &nb_out, cur_tsc);
			sw_drop += (nb_rx - nb_out);

			if (likely(nb_out > 0)) {
				/* Send burst of TX packets, to second port of pair. */
				const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0, out, nb_out);
				/* Free any unsent packets. */
				if (unlikely(nb_tx < nb_out)) {
					for (uint16_t buf = nb_tx; buf < nb_out; buf++) {
						rte_pktmbuf_free(out[buf]);
						sw_drop++;
					}
				}
				/* Update the SW statistics */
				for (uint16_t i = 0; i < nb_tx; i++) {
					if (port == 0) {
						sw_stats_up[1].ibytes +=  rte_pktmbuf_pkt_len(out[i]) + 24;
						sw_stats_up[1].ipackets++;
					} else {
						sw_stats_down[1].ibytes +=  rte_pktmbuf_pkt_len(out[i]) + 24;
						sw_stats_down[1].ipackets++;
					}
				}
			}
		}
    total_tsc+=(cur_tsc-prev_tsc);
    if (b) {
      // We did something
      handlingpkts_tsc += (cur_tsc-prev_tsc);
    }
    prev_tsc = cur_tsc;
  }
	flow_classification_destroy(handle);
  return 0;
}

/* Initialise a single port on an Ethernet device */
static void
init_port(uint8_t port)
{
	int ret;

	/* Options for configuring ethernet port */
	const struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_rx_pkt_len = 9000,
			.jumbo_frame = 1,       /* Jumbo Frame Support enabled */
		},
   .fdir_conf = {
			.mode = RTE_FDIR_MODE_PERFECT,
			.status = RTE_FDIR_REPORT_STATUS,
			.drop_queue = 127,
		},
	};

	/* Initialise device and RX/TX queues */
	printf("Initialising port %u ...", (unsigned)port);
	fflush(stdout);
	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret < 0)
		fprintf(stderr, "Could not configure port%u (%d)", (unsigned)port, ret);

	ret = rte_eth_rx_queue_setup(port, 0, NUM_RXD, rte_eth_dev_socket_id(port),
	        NULL, pktmbuf_pool);
	if (ret < 0)
		fprintf(stderr, "Could not setup up RX queue for port%u (%d)",
		       (unsigned)port, ret);

	ret = rte_eth_tx_queue_setup(port, 0, NUM_TXD, rte_eth_dev_socket_id(port),
				NULL);
	if (ret < 0)
		fprintf(stderr, "Could not setup up TX queue for port%u (%d)",
		        (unsigned)port, ret);

	ret = rte_eth_dev_start(port);
	if (ret < 0)
		fprintf(stderr, "Could not start port%u (%d)", (unsigned)port, ret);

	rte_eth_promiscuous_enable(port);
}

/* Initialise ports/queues etc. and start main loop on each core */
int
main(int argc, char** argv)
{
	int ret;
	uint8_t nb_sys_ports, port;

	/* Associate signal_hanlder function with USR signals */
	signal(SIGALRM, signal_handler);
	signal(SIGINT, signal_handler);

	/* Initialise EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "Could not initialise EAL (%d)", ret);
		return -1;
	}
	argc -= ret;
	argv += ret;

	/* Create the mbuf pool */
	pktmbuf_pool = rte_mempool_create("mbuf_pool", NUM_MBUFS, MBUF_SIZE,
																		MEMPOOL_CACHE_SIZE,
																		sizeof(struct rte_pktmbuf_pool_private),
																		rte_pktmbuf_pool_init, NULL,
																		rte_pktmbuf_init, NULL,
																		rte_socket_id(), 0);
	if (pktmbuf_pool == NULL) {
		fprintf(stderr, "Could not initialise mbuf pool");
		return -1;
	}

	/* Get number of ports found in scan */
	nb_sys_ports = rte_eth_dev_count();
	if (nb_sys_ports == 0) {
		fprintf(stderr, "No supported Ethernet device found");
		return -1;
	}

	if (nb_sys_ports != 2) {
		fprintf(stderr, "Need 2 ports");
		return -1;
	}

if (rte_lcore_count() != 3)
		rte_exit(EXIT_FAILURE, "Need three lcores (only)!\n");

	/* Initialise each port */
	for (port = 0; port < NUM_PORTS; port++) {
		init_port(port);
	}

	printf("\n");

	/* Launch the main_loop on the available lcore*/
	int id_core = rte_lcore_id();
	id_core = rte_get_next_lcore(id_core, 1, 1);
	printf("Running on core %d\n", id_core);
	rte_eal_remote_launch(main_loop, NULL, id_core);


	/* Stay here until all lcores terminate */
	rte_eal_mp_wait_lcore();
	return 0;
}
