# DPDK Contiguous Batching
The branch introduce an optimization to the pdump library and the pcap PMD.

## PCAP PMD
The PCAP PMD, rte_eth_pcap.c, optimization is basically just introducing a 
buffered write instead of writing each packet to disk one at a time.

## PDUMP
The PDUMP optimization is introducing contiguous batching which will pack 
multiple packets back-to-back within a rte_mbuf. The benefit of this is fever 
exchanges in the rte_rings and better memory utilization along with improved L1 
prefetching because the HW prefetcher is utilized and there is no need for the 
SW prefetching hint.

## How to make/test
In order to test the changes, dpdk must be build with the following flags:
```bash
make EXTRA_CFLAGS="-DUSE_BATCHING -DUSE_STDOUT_STATISTICS"
```
The USE_BATCHING define is enabling the contiguous batching and the 
STDOUT_STATISTICS is enabling a throughput statistics on stdout of the 
application using the PCAP PMD, which in this case would be the pdump app.

## Results
The results are documented in the following [blog](http://www.napatech.com/smarterdatadelivery/dpdk-packet-capture-pdump):



