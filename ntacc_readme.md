# Napatech PCI Poll Mode driver – NTACC PMD.
----------
The Napatech NTACC PMD enables users to run DPDK on top of the Napatech adapters and driver. The NTACC PMD is a PCI driver.

The NTACC PMD driver does not need to be bound. This means that the dpdk-devbind-py script cannot be used to bind the interface. The DPDK app will automatically find and use the NTACC PMD driver when starting, provided that the Napatech driver is started and the Napatech adapter is not blacklisted.

## Table of Contents
1. [Napatech Driver](#driver)
2. [Compiling the Napatech NTACC PMD driver](#compiling)
	1. [Environment variable](#Environment)
	2. [Configuration setting](#configuration)
3. [Napatech Driver Configuration](#driverconfig)
	1. [Statistics update interval](#statinterval)
4. [Number of RX queues and TX queues available](#queues)
5. [Starting NTACC PMD](#starting)
6. [Priority](#Priority)
7. [Generic rte_flow filter items](#genericflow)
8. [Generic rte_flow RSS/Hash functions](#hash)
9. [Generic rte_flow filter Attributes](#attributes)
10. [Generic rte_flow filter actions](#actions)
11. [Generic rte_flow RSS/Hash functions](#hash)
	1. [Symmetric/Unsymmetric hash](#Symmetric)
	2. [Default RSS/HASH function](#DefaultRSS)
12. [Default filter](#DefaultFilter)
	1. [Disabling default filter](#DisablingDefaultFilter)
13. [Examples of generic rte_flow filters](#examples1)
14. [Limited filter resources](#resources)
15. [Filter creation example -  5tuple filter](#Filtercreationexample)
16. [Filter creation example - Multiple 5tuple filter (IPv4 addresses and TCP ports)](#examples2)
17. [Copy packet offset to mbuf](#copyoffset)
18. [Use NTPL filters addition (Making an ethernet over MPLS filter)](#ntplfilter)
19.  [Batching - Receive a batch of packets](#batching)
	1. [mbuf changes](#mbuf)
	2. [Batching buffer](#batchbuf)
	3. [Browsing the batch buffer directly](#browbatchbuf)
	4. [Browsing the batch buffer using mbuf helper function](#browhelper)
	5. [Helper functions](#helperfunc)
		1. [rte_pktmbuf_batch_get_next_packet](#getnext)
		2. [rte_pktmbuf_batch_copy_packet_from_batch](#copybatch)
		3. [rte_pktmbuf_batch_copy_packet_from_mbuf](#copymbuf)
	6. [Batching example](#batchexam)

## Napatech Driver <a name="driver"></a>

The Napatech driver and adapter must be installed and started before the NTACC PMD can be used. See the installation guide in the Napatech driver package for how to install and start the driver.

See below for supported drivers and adapters:

|  Supported drivers |
|-------------------------|
| 3.7.X                      |

<br>

|  Supported adapters                        | FPGA                        |
|---------------------------------------------------|---------------------------|
|  NT40A01-01-SCC-4×1-E3-FF-ANL        |  200-9500-09-08-00 |
|  NT20E3-2-PTP-ANL                               |  200-9501-09-08-00 |
|  NT40E3-4-PTP-ANL                               |  200-9502-09-08-00 |
|  NT80E3-2-PTP-ANL                               |  200-9503-09-08-00 |
|  NT100E3‐1‐PTP‐ANL                             |  200-9505-09-08-00 |
|  NT200A01-02-SCC-2×40-E3-FF-ANL    |  200-9512-09-08-00 |
|  NT200A01-02-SCC-2×100-E3-FF-ANL  |  200-9515-09-08-00 |

The complete driver package can be downloaded here:
[ntanl_package_3gd_linux_10.0.1](https://support.napatech.com/Releases/SmartNICs/10.0.1/ntanl_package_3gd_linux_10.0.1.tar.gz) 


## Compiling the Napatech NTACC PMD driver <a name="compiling"></a>

##### Environment variable <a name="Environment"></a>
In order to compile the NTACC PMD, the NAPATECH3_PATH environment variable must be set. This tells DPDK where the Napatech driver is installed.

`export NAPATECH3_PATH=/opt/napatech3`

/opt/napatech3 is the default path for installing the Napatech driver. If the driver is installed elsewhere, that path must be used.

##### Configuration setting  <a name="configuration"></a>
To enable DPDK to compile NTACC PMD, a configuration setting must be set in the file common_base.

`CONFIG_RTE_LIBRTE_PMD_NTACC=y`

Three other configurations settings can be used to change the behaviour of the NTACC PMD:

- Hardware based or software based statistic:
This setting is used to select between software based and hardware based statistics.
<br>`CONFIG_RTE_LIBRTE_PMD_NTACC_USE_SW_STAT=n`

- Disable default filter:
This setting is used to disable generation of a default catch all filter. See [Default filter](#default-filter) for further information.
<br>`CONFIG_RTE_LIBRTE_PMD_NTACC_DISABLE_DEFAULT_FILTER=n`

- Copy offset:
This setting is used to copy offset to different packets layers into the mbuf. See [Copy packet offset to mbuf](#copyoffset) for further information.
<br>`CONFIG_RTE_LIBRTE_PMD_NTACC_COPY_OFFSET=y`

## Napatech Driver Configuration <a name="driverconfig"></a>
The Napatech driver is configured using an ini-file – `ntservice.ini`. By default, the ini-file is located in `/opt/napatech3/config`. The following changes must be made to the default ini-file.

The Napatech driver uses hostbuffers to receive and transmit data. The number of hostbuffers must be equal to or larger than the number of RX and TX queues used by the DPDK.

To change the number of hostbuffers, edit ntservice.ini and change following keys:

	HostBuffersRx = [R1, R2, R3]
	HostBuffersTx = [T1, T2, T3]

Hostbuffer settings:

| Parameter | Description                                  |                                                                                                                                          |
|-----------|----------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| R1        |  Number of RX hostbuffers                    | Must be equal to or larger than the number of RX queues used.<br>Maximum value = 128.                                                    | 
| R2        |  Size of a hostbuffer in MB                  | The optimal size depends of the use. The default is usually fine.<br>The value must be a multiple of 4 and larger or equal to 16.        |
| R3        |  Numa node where the hostbuffer is allocated | Use -1 for NUMA node autodetect. Hostbuffers will be allocated from the NUMA node to which the adapter is connected.                 |
| T1        |  Number of TX hostbuffers                    | Must be equal to or larger than the number of TX queues used.<br>Maximum value = 128.                                                    | 
| T2        |  Size of a hostbuffer in MB                  | The optimal size depends of the use. The default is usually fine.<br>The value must be a multiple of 4 and larger or equal to 16.        |
| T3        |  Numa node where the hostbuffer is allocated | Use -1 for NUMA node autodetect. Hostbuffers will be allocated from the NUMA node to which the adapter is connected.                 |

The default setting for adapter X is:

```
[AdapterX]
HostBuffersRx = [4, 16, -1]
HostBuffersTx = [4, 16, -1]
```
This means that it will be possible to create 4 RX queues and 4 TX queues.

#### Statistics update interval  <a name="statinterval"></a>
When using hardware based statistics `CONFIG_RTE_LIBRTE_PMD_NTACC_USE_SW_STAT=n`, the default update interval is 500 ms i.e. the time between each time the statistics is updated by the adapter. In some cases, the update interval is too large. The update interval can be changed by changing the `StatInterval` option in the system section in the ini-file.
```
[System]
StatInterval=1
```

Possible values in milliseconds for StatInterval are:
`1, 10, 20, 25, 50, 100, 200, 400, 500`

> Note: Increasing the statistics update frequency requires more CPU cycles.

## Number of RX queues and TX queues available <a name="queues"></a>

Up to 128 RX queues are supported. They are distributed between the ports on the Napatech adapter and rte_flow filters on a first-come, first-served basis.

Up to 128 TX queues are supported. They are distributed between the ports on the Napatech adapter on a first-come, first-served basis.

The maximum number of RX queues per port are the smallest number of either:

- (256 / number of ports)
- `RTE_ETHDEV_QUEUE_STAT_CNTRS`

`RTE_ETHDEV_QUEUE_STAT_CNTRS` is defined in `common_base`

## Starting NTACC PMD <a name="starting"></a>

When a DPDK app is starting, the NTACC PMD is automatically found and used by the DPDK. All Napatech adapters installed and activated will appear in the DPDK app. To use only some of the installed Napatech adapters, the whitelist command must be used. The whitelist command is also used to select specific ports on an adapter.

| whitelist command format |  Description |
|-----------------------------------|---|
| `-w <[domain:]bus:devid.func>` | Select a specific PCI adapter |
| `-w <[domain:]bus:devid.func>,mask=X` | Select a specific PCI adapter, <br>but use only the ports defined by mask<br>The mask command is specific for Napatech adapters |



Example 1:
A NT40E3-4-PTP-ANL Napatech adapter is installed. We want to use only port 0 and 1.

- `DPDKApp  -w 0000:82:00.0,mask=3`

Example 2:
Two NT40E3-4-PTP-ANL Napatech adapters are installed. We want to use only port 0 and 1 on adapter 1 and only port 2 and 3 on adapter 2.

- `DPDKApp  -w 0000:82:00.0,mask=3 -w 0000:84:00.0,mask=0xC`

Example 3:
A NT40E3-4-PTP-ANL Napatech adapter is installed. We want app1 to use only port 0 and 1, and app2 to use only port 2 and 3. In order to start two DPDK applications, we must share the memory between the two applications using --file-prefix and --socket-mem. Note that both applications will get DPDK port number 0 and 1, even though app2 will use port number 2 and 3 on the adapter.

- `DPDKApp1  -w 0000:82:00.0,mask=3 --file-prefix fc0 --socket-mem 1024,1024`
- `DPDKApp2  -w 0000:82:00.0,mask=12 --file-prefix fc1 --socket-mem 1024,1024`

<br>
> Note: When using the whitelist command, all adapters to be used must be included.

The Napatech adapters can also be disabled by using the blacklist command.
 
## Generic rte_flow filter items <a name="genericflow"></a>

The NTACC PMD driver supports a number of generic rte_flow filters including some Napatech defined filters.

Following rte_flow filters are supported:

| rte_flow filter	      | Supported fields                                                                                                                                                            | 
|-------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|`RTE_FLOW_ITEM_TYPE_END`   |                                                                                                                                                                             |
|`RTE_FLOW_ITEM_TYPE_VOID`  |                                                                                                                                                                             |
|`RTE_FLOW_ITEM_TYPE_ETH`   |	`src.addr_bytes`<br>`dst.addr_bytes`<br>`type`                                                                                                                                    |
|`RTE_FLOW_ITEM_TYPE_IPV4`  |	`hdr.version_ihl`<br>`hdr.type_of_service`<br>`hdr.total_length`<br>`hdr.packet_id`<br>`hdr.fragment_offset`<br>`hdr.time_to_live`<br>`hdr.next_proto_id`<br>`hdr.dst_addr`<br>`hdr.src_addr` |
|`RTE_FLOW_ITEM_TYPE_IPV6`  | `hdr.vtc_flow`<br>`hdr.proto`<br>`hdr.hop_limits`<br>`hdr.src_addr`<br>`hdr.dst_addr`                                                                                                 |
|`RTE_FLOW_ITEM_TYPE_SCTP`  | `hdr.src_port`<br>`hdr.dst_port`                                                                                                                                               |
|`RTE_FLOW_ITEM_TYPE_TCP`   | `hdr.src_port`<br>`hdr.dst_port`<br>`hdr.data_off`<br>`hdr.tcp_flags`                                                                                                               |
|`RTE_FLOW_ITEM_TYPE_UDP`   | `hdr.src_port`<br>`hdr.dst_port`                                                                                                                                                |
|`RTE_FLOW_ITEM_TYPE_ICMP`  | `hdr.icmp_type`<br>`hdr.icmp_code`                                                                                                                                              |
|`RTE_FLOW_ITEM_TYPE_VLAN`  | `tpid`<br>`tci`                                                                                                                                                                 |
|`RTE_FLOW_ITEM_TYPE_NVGRE` | Only packet type = `NVGRE`                                                                                                                                                    |
|`RTE_FLOW_ITEM_TYPE_VXLAN` | Only packet type = `VXLAN`                                                                                                                                                    |
| `RTE_FLOW_ITEM_TYPE_GRE`  | `c_rsvd0_ver` (only version = bit b0-b2)|
| `RTE_FLOW_ITEM_TYPE_PORT`  | `index`<br>The port numbers used, must be the local port numbers for the adapter. <br>For a 4 port adapter the port numbers are 0 to 3.|

The following rte_flow filters are added by Napatech and are not a part of the main DPDK:

| rte_flow filter	         | Supported fields           | 
|----------------------------|----------------------------|
|`RTE_FLOW_ITEM_TYPE_GREv0`    | Only packet type = `GREv0`   |
|`RTE_FLOW_ITEM_TYPE_GREv1`    | Only packet type = `GREv1`   |
|`RTE_FLOW_ITEM_TYPE_GTPv1_U`  | Only packet type = `GTPv1_U` |
|`RTE_FLOW_ITEM_TYPE_GTPv0_U`  | Only packet type = `GTPv0_U` |
|`RTE_FLOW_ITEM_TYPE_IPinIP`   | Only packet type = `IPinIP`  |
|`RTE_FLOW_ITEM_TYPE_NTPL`   | [see *Use NTPL filters*](#ntplfilter)  |

**Supported fields**: The fields in the different filters that can be used when creating a rte_flow filter.

**Only packet type**: No fields can be used. The filter will match packets containing this type of tunnel.

## An example of an inner (tunnel) filter

All flow items defined after a tunnel flow item will be an inner filter, as shown below:

1. `RTE_FLOW_ITEM_TYPE_ETH`: Outer ether filter
2. `RTE_FLOW_ITEM_TYPE_IPV4`: Outer IPv4 filter
3. `RTE_FLOW_ITEM_TYPE_NVGRE`: Tunnel filter
4. `RTE_FLOW_ITEM_TYPE_ETH`: Inner ether filter (because of the tunnel filter)
5. `RTE_FLOW_ITEM_TYPE_IPV4`: Inner IPv4 filter (because of the tunnel filter)

## Generic rte_flow filter Attributes <a name="attributes"></a>

The following rte_flow filter attributes are supported:

| Attribute     | Values                                                 |
|---------------|--------------------------------------------------------|
|`priority`     | 0 – 62<br>Highest priority = 0<br>Lowest priority = 62 |
|`ingress`      |                                                        |

## Priority <a name="Priority"></a>
If multiple filters are used, priority is used to select the order of the filters. The filter with the highest priority will always be the filter to be used. If filters overlap, for example an ethernet filter sending the packets to queue 0 and an IPv4 filter sending the packets to queue 1 (filters overlap as IPv4 packets are also ethernet packets), then the filter with the highest priority is used. 

If the ethernet filter has the highest priority, all packets will go to queue 0 and no packets will go to queue 1.

If the IPv4 filter has the highest priority, all IPv4 packets will go to queue 1 and all other packets will go to queue 0.

If the filters have the samme priority, the filter entered last is the one to be used.

## Generic rte_flow filter actions <a name="actions"></a>

Following rte_flow filter actions are supported:

| Action                      | Supported fields                        |
|-----------------------------|-----------------------------------------|
|`RTE_FLOW_ACTION_TYPE_END`   |                                         |
|`RTE_FLOW_ACTION_TYPE_VOID`  |                                         |
|`RTE_FLOW_ACTION_TYPE_MARK`  | See description below                   |
|`RTE_FLOW_ACTION_TYPE_RSS`   | `num`<br>`queue`<br>`rss_conf-> rss_hf` |
|`RTE_FLOW_ACTION_TYPE_QUEUE` | `index`                                 |

- `RTE_FLOW_ACTION_TYPE_MARK`
  - If MARK is set and a packet matching the filter is received, the mark value will be copied to mbuf->hash.fdir.hi and the PKT_RX_FDIR_ID flag in mbuf->ol_flags is set.
  - If a packet not matching the filter is received, the HASH value of the packet will be copied to mbuf->hash.rss and the PKT_RX_RSS_HASH flag in mbuf->ol_flags is set.

Note: Currently maximum MARK value is 0x1FFF (13 bit).

- `RTE_FLOW_ACTION_TYPE_RSS`
The supported HASH function is described below.

## Generic rte_flow RSS/Hash functions <a name="hash"></a>

The following rte_flow filter HASH functions are supported:

| HASH function	| | HASH Keys |
|------|--|------|
|`ETH_RSS_IPV4`| `2-tuple` | `IPv4: hdr.src_addr`<br>`IPv4: hdr.dst_addr` |
|`ETH_RSS_NONFRAG_IPV4_TCP`	|`5-tuple`|`IPv4: hdr.src_addr`<br>`IPv4: hdr.dst_addr`<br>`IPv4: hdr.next_proto_id`<br>`TCP: hdr.src_port`<br>`TCP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV4_UDP`|`5-tuple`|`IPv4: hdr.src_addr`<br>`IPv4: hdr.dst_addr`<br>`IPv4: hdr.next_proto_id`<br>`UDP: hdr.src_port`<br>`UDP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV4_SCTP`|`5-tuple`|`IPv4: hdr.src_addr`<br>`IPv4: hdr.dst_addr`<br>`IPv4: hdr.next_proto_id`<br>`SCTP: hdr.src_port`<br>`SCTP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV4_OTHER`|`2-tuple`|`IPv4: hdr.src_addr`<br>`IPv4: hdr.dst_addr`|
|`ETH_RSS_IPV6`|`2-tuple`|`IPv6: hdr.src_addr`<br>`IPv6: hdr.dst_addr`|
|`ETH_RSS_NONFRAG_IPV6_TCP`|`5-tuple`|`IPv6: hdr.src_addr`<br>`IPv6: hdr.dst_addr`<br>`IPv6: hdr.proto`<br>`TCP: hdr.src_port`<br>`TCP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV6_UDP`|`5-tuple`|`IPv6: hdr.src_addr`<br>`IPv6: hdr.dst_addr`<br>`IPv6: hdr.proto`<br>`UDP: hdr.src_port`<br>`UDP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV6_SCTP`|`5-tuple`|`IPv6: hdr.src_addr`<br>`IPv6: hdr.dst_addr`<br>`IPv6: hdr.proto`<br>`SCTP: hdr.src_port`<br>`SCTP: hdr.dst_port`|
|`ETH_RSS_NONFRAG_IPV6_OTHER`|`2-tuple`|`IPv6: hdr.src_addr`<br>`IPv6: hdr.dst_addr`|

The following rte_flow filter HASH functions are added by Napatech and is not a part of the main DPDK:

| HASH function | Description |
|------|------|
|`ETH_RSS_INNER_IPV4`| As `ETH_RSS_IPV4`, but hashing is done on the inner IPv4.|
|`ETH_RSS_INNER_IPV4_TCP`| As `ETH_RSS_NONFRAG_IPV4_TCP`, but hashing is done on the inner IPv4 and TCP.|
|`ETH_RSS_INNER_IPV4_UDP`| As `ETH_RSS_NONFRAG_IPV4_UDP`, but hashing is done on the inner IPv4 and UDP.|
|`ETH_RSS_INNER_IPV4_SCTP`| As `ETH_RSS_NONFRAG_IPV4_SCTP`, but hashing is done on the inner IPv4 and SCTP.|
|`ETH_RSS_INNER_IPV4_OTHER`| As `ETH_RSS_NONFRAG_IPV4_OTHER`, but hashing is done on the inner IPv4.|
|`ETH_RSS_INNER_IPV6`| As `ETH_RSS_IPV6`, but hashing is done on the inner IPv6.|
|`ETH_RSS_INNER_IPV6_TCP`| As `ETH_RSS_NONFRAG_IPV6_TCP`, but hashing is done on the inner IPv6 and TCP.|
|`ETH_RSS_INNER_IPV6_UDP`| As `ETH_RSS_NONFRAG_IPV6_UDP`, but hashing is done on the inner IPv6 and UDP.|
|`ETH_RSS_INNER_IPV6_SCTP`| As `ETH_RSS_NONFRAG_IPV6_SCTP`, but hashing is done on the inner IPv6 and SCTP.|
|`ETH_RSS_INNER_IPV6_OTHER`| As `ETH_RSS_NONFRAG_IPV6_OTHER`, but hashing is done on the inner IPv6.|

##### Symmetric/Unsymmetric hash <a name="Symmetric"></a>
The key generation can either be symmetric (sorted) or unsymmetric (unsorted). This is selected by the function:

```
int rte_eth_dev_filter_ctrl(uint8_t port_id, enum rte_filter_type filter_type, enum rte_filter_op filter_op, void *arg);
```

in the following way:

```C++
struct rte_eth_hash_filter_info info;
memset(&info, 0, sizeof(info));
info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
info.info.enable = 0;
if (rte_eth_dev_filter_ctrl(0, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info) < 0) {
  printf("Cannot set symmetric hash enable per port on port %u\n", 0);
  return NULL;
}
```

Setting `info.info.enable = 0` disables symmetric hash and setting `info.info.enable = 1` enables symmetric hash.

> `sorted` and `unsorted` is Napatech terms for symmetric and unsymmetric. 
 


##### Default RSS/HASH function <a name="DefaultRSS"></a>
A default RSS/HASH function can be setup when configuring the ethernet device by using the function:

	int rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue, uint16_t nb_tx_queue, const struct rte_eth_conf *eth_conf);

Using the parameter `eth_conf` of the type `struct rte_eth_conf`, a default RSS/HASH function can be defined.  

Following two fields of the `struct rte_eth_conf` are supported:

- `rxmode.mq_mode`
- `rx_adv_conf.rss_conf.rss_hf`

To setup a default RSS/HASH function use:

```C++
static const struct rte_eth_conf port_conf_default = {
  .rxmode = {
	.mq_mode = ETH_MQ_RX_RSS,
  },
    .rx_adv_conf = {
      .rss_conf = {
      .rss_hf = ETH_RSS_IP,
    },
  }
};
	
/* Configure the Ethernet device. */
retval = rte_eth_dev_configure(port, rx_queues, tx_queues, &port_conf_default);
if (retval != 0)
	return retval;
```

See below for information about the consequences of setting a default RSS/HASH function


## Default filter <a name="DefaultFilter"></a>
When starting the NTACC PMD driver, a default catch all filter with priority 62 (lowest priority) is created for each DPDK port. The filter will send all incoming packets to queue 0 for each DPDK port. If rte_flow filters are created with higher priority, then all packets matching these filters will be send to the queues defined by the filters. All packets not matching the filter will be send to queue 0.

If a default RSS (hash) mode is defined using the rte_eth_dev_configure command (mq_mode = ETH_MQ_RX_RSS), a default catch all filter is created, that will send incoming packet to all defined queues using the defined RSS/HASH function. 

> With a default RSS/HASH function, the default filter will collide with any rte_flow filters created, as all non matched packets will be distributed to all defined queues using the default RSS/HASH function. It is recommended not to define a default RSS/HASH function if any rte_flow filters are going to be used.


#### Disabling default filter <a name="DisablingDefaultFilter"></a>
The default filter can be disabled either at compile time by setting:

`CONFIG_RTE_LIBRTE_PMD_NTACC_DISABLE_DEFAULT_FILTER=y`

or at runtime by using the rte_flow_isolate command:

| Action                       | Command                                      |
|---------------------------|---------------------------------------------|
| Disable default filter |`rte_flow_isolate(portid, 1, error) ` |
| Enable default filter  | `rte_flow_isolate(portid, 0, error) `|


## Examples of generic rte_flow filters <a name="examples1"></a>

Some examples of how to use the generic rte_flow is shown below.

Setting up an IPV4 filter:
```C++
attr.ingress = 1;
attr.priority = 1;
		
struct rte_flow_action_queue flow_queue =  { 0 };
actions[actionCount].type = RTE_FLOW_ACTION_TYPE_QUEUE;
actions[actionCount].conf = &flow_queue;
actionCount++;
		
struct rte_flow_item_ipv4 ipv4_spec = {
  .hdr = {
    .src_addr = rte_cpu_to_be_32(IPv4(192,168,20,108)),
    .dst_addr = rte_cpu_to_be_32(IPv4(159,20,6,6)),
  }
};
		
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
pattern[patternCount].spec = &ipv4_spec;
patternCount++;
flow = rte_flow_create(0, &attr, pattern, actions, &error);
```

An outer ether filter and an inner IPV4 filter:
```C++
attr.ingress = 1;
attr.priority = 1;
	
struct rte_flow_action_queue flow_queue =  { 0 };
actions[actionCount].type = RTE_FLOW_ACTION_TYPE_QUEUE;
actions[actionCount].conf = &flow_queue;
actionCount++;
	
struct rte_flow_item_eth item_eth_spec = {
  .src = "\x04\xf4\xbc\x04\x26\xa5",
  .dst = "\x22\x33\x44\x55\x66\x77",
};
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_ETH;
pattern[patternCount].spec = &item_eth_spec;
patternCount++;
	
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_GTPV1_U; // The following filters
	                                                         // are inner filters
patternCount++;
	
struct rte_flow_item_ipv4 ipv4_spec	= {
  .hdr = {
    .src_addr = rte_cpu_to_be_32(IPv4(192,168,20,108)),
    .dst_addr = rte_cpu_to_be_32(IPv4(159,20,6,6)),		
  }
};
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
pattern[patternCount].spec = &ipv4_spec;
patternCount++;
flow = rte_flow_create(0, &attr, pattern, actions, &error);
```

All rte_flow filters added in same rte_flow_create will be and’ed together. To create filters that are or’ed together, call rte_flow_create for each filter.

## Limited filter resources <a name="resources"></a>

The Napatech adapter and driver has a limited number of filter resources when using the generic rte_flow filter. In some cases, a filter cannot be created. In these cases, it will be necessary to simplify the filter.

## Filter creation example <a name="Filtercreationexample"></a>
The following example creates a 5tuple IPv4/TCP filter. If `nbQueues > 1` RSS/Hashing is made to the number of queues using hash function `ETH_RSS_IPV4`. Symmetric hashing is enabled. Packets are marked with 12.
```C++
static struct rte_flow *SetupFilter(uint8_t portid, uint8_t nbQueues)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[10];
	struct rte_flow_action actions[10];
	struct rte_flow_error error;
	struct rte_eth_hash_filter_info info;
	uint32_t patternCount = 0;
	uint32_t actionCount = 0;

	struct rte_flow *flow = NULL;
	struct rte_flow_action_rss *rss = NULL;
	struct rte_eth_rss_conf rss_conf;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x22, sizeof(error));

	memset(&attr, 0, sizeof(attr));
	attr.ingress = 1;  // Must always be 1
	attr.priority = 1;

	memset(&pattern, 0, sizeof(pattern));

	// Create IPv4 filter
	const struct rte_flow_item_ipv4 ipv4_spec = {
		.hdr = {
			.src_addr = rte_cpu_to_be_32(IPv4(10,0,0,9)),
			.dst_addr = rte_cpu_to_be_32(IPv4(10,0,0,2)),
			.next_proto_id = 6,
		},
	};
	const struct rte_flow_item_ipv4 ipv4_mask = {
		.hdr = {
			.src_addr = rte_cpu_to_be_32(IPv4(255,255,255,255)),
			.dst_addr = rte_cpu_to_be_32(IPv4(255,255,255,255)),
			.next_proto_id = 255,
		},
	};
	pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[patternCount].spec = &ipv4_spec;
	pattern[patternCount].mask = &ipv4_mask;
	patternCount++; 

	// Create TCP filter
	const struct rte_flow_item_tcp tcp_spec = {
		.hdr = {
			.src_port = rte_cpu_to_be_16(34567),
			.dst_port = rte_cpu_to_be_16(152),
		},
	};
	const struct rte_flow_item_tcp tcp_mask = {
		.hdr = {
			.src_port = rte_cpu_to_be_16(0xFFFF),
			.dst_port = rte_cpu_to_be_16(0xFFFF),
		},
	};

	pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TCP;
	pattern[patternCount].spec = &tcp_spec;
	pattern[patternCount].mask = &tcp_mask;
	patternCount++; 

	pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;

	memset(&actions, 0, sizeof(actions));

	if (nbQueues <= 1) {
		// Do only use one queue
		struct rte_flow_action_queue flow_queue0 =  { 0 };
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_QUEUE;
		actions[actionCount].conf = &flow_queue0;
		actionCount++;
	}
	else {
		// Use RSS (Receive side scaling) to nbQueues queues
		int i;
		rss = malloc(sizeof(struct rte_flow_action_rss) + sizeof(uint16_t) * nbQueues);
		if (!rss) {
			printf("Memory allocation error\n");
			return NULL;
		}

		rss_conf.rss_key = NULL;   // Not supported
		rss_conf.rss_key_len = 0;  // Not supported
		rss_conf.rss_hf = ETH_RSS_IPV4; // IPV4 5tuple hashing

		rss->num = nbQueues;
		for (i = 0; i < nbQueues; i++) {
			rss->queue[i] = i;
		}
		rss->rss_conf = &rss_conf;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
		actions[actionCount].conf = rss;
		actionCount++;
	}

	// Marks packets with 12
	struct rte_flow_action_mark mark;
	mark.id = 12;
	actions[actionCount].type = RTE_FLOW_ACTION_TYPE_MARK;
	actions[actionCount].conf = &mark;
	actionCount++;

	actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
	actions[actionCount].conf = NULL;

	// Set symmetric hashing - Note this must be done before 
	// rte_flow_create is called.
	memset(&info, 0, sizeof(info));
	info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
	info.info.enable = 1;
	if (rte_eth_dev_filter_ctrl(0, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info) < 0) {
		printf("Cannot set symmetric hash enable per port on port %u\n", 0);
		if (rss) {
			free(rss);
		}
		return NULL;
	}

	flow = rte_flow_create(portid, &attr, pattern, actions, &error);
	if (!flow) {
		printf("Filter error: %s\n", error.message);
		if (rss) {
			free(rss);
		}
		return NULL;
	}

	if (rss) {
		free(rss);
	}
	return flow;
}
```

## Filter creation example - Multiple 5tuple filter (IPv4 addresses and TCP ports) <a name="examples2"></a>
The following example shows how it is possible to create a 5tuple filter matching on a large number of IPv4 addresses and TCP ports. 

The commands used in the loop to program the IP addresses and tcp ports must be the same for all addresses and ports. The only things that must be changed are the values for IP addresses and tcp ports.

The driver is then able to optimize the final filter making it possible to have up to 20.000 filter items. 

> Note: The number of possible filter items depends on the filter made and will vary depending on the complexity of the filter. 

> Note: The way the filter in this example is made, can be used for all rte_flow_items as long as only the filter values are changed.

```C++
struct ipv4_adresses_s {
	uint32_t src_addr;		
	uint32_t dst_addr;		
	uint16_t tcp_src;
	uint16_t tcp_dst;
};

#define NB_ADDR 10
static struct ipv4_adresses_s ip_table[NB_ADDR] = {
	{ IPv4(10,10,10,1),  IPv4(172,217,19,206), 32414, 80 },
	{ IPv4(10,10,10,2),  IPv4(172,217,19,206), 30414, 80  },
	{ IPv4(10,10,10,3),  IPv4(172,217,19,206),  2373, 80  },
	{ IPv4(10,10,10,4),  IPv4(172,217,19,206), 42311, 80  },
	{ IPv4(10,10,10,5),  IPv4(172,217,19,206),   414, 80  },
	{ IPv4(10,10,10,6),  IPv4(172,217,19,206),  2514, 80  },
	{ IPv4(10,10,10,7),  IPv4(172,217,19,206), 35634, 80  },
	{ IPv4(10,10,10,8),  IPv4(172,217,19,206), 35779, 80  },
	{ IPv4(10,10,10,9),  IPv4(172,217,19,206), 23978, 80  },
	{ IPv4(10,10,10,10), IPv4(172,217,19,206), 19634, 80  },
};

static int SetupFilter(uint8_t portid, struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[100];
	struct rte_flow_action actions[10];
	struct rte_eth_hash_filter_info info;
	struct rte_flow *flow;

	// Pattern struct
	struct rte_flow_item_ipv4 ipv4_spec;
	memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));

	static struct rte_flow_item_tcp tcp_spec;
	memset(&tcp_spec, 0, sizeof(struct rte_flow_item_tcp));

	// Action struct
	struct rte_eth_rss_conf rss_conf;

	struct {
		struct rte_flow_action_rss rss;
		uint16_t queue[4];
	} test;

	static struct rte_flow_action_mark mark;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(error, 0x22, sizeof(struct rte_flow_error));

	memset(&attr, 0, sizeof(attr));
	attr.ingress = 1;
	attr.priority = 1;

	for (unsigned i = 0; i < NB_ADDR; i++) {
		uint32_t patternCount = 0;
		memset(&pattern, 0, sizeof(pattern));

		ipv4_spec.hdr.src_addr = rte_cpu_to_be_32(ip_table[i].src_addr);
		ipv4_spec.hdr.dst_addr = rte_cpu_to_be_32(ip_table[i].dst_addr);
		pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
		pattern[patternCount].spec = &ipv4_spec;
		patternCount++;

		tcp_spec.hdr.src_port = rte_cpu_to_be_16(ip_table[i].tcp_src);
		tcp_spec.hdr.dst_port = rte_cpu_to_be_16(ip_table[i].tcp_dst);
		pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TCP;
		pattern[patternCount].spec = &tcp_spec;
		patternCount++;

		pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END; 
		patternCount++;

		uint32_t actionCount = 0;
		memset(&actions, 0, sizeof(actions));

		rss_conf.rss_key = NULL;
		rss_conf.rss_key_len = 0;
		rss_conf.rss_hf = ETH_RSS_NONFRAG_IPV4_TCP;

		test.rss.num = 4;
		test.rss.queue[0] = 0;
		test.rss.queue[1] = 1;
		test.rss.queue[2] = 2;
		test.rss.queue[3] = 3;
		test.rss.rss_conf = &rss_conf;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
		actions[actionCount].conf = &test.rss;
		actionCount++;

		mark.id = 123;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_MARK;
		actions[actionCount].conf = &mark;
		actionCount++;

		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
		actionCount++;

		memset(&info, 0, sizeof(info));
		info.info_type = RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT;
		info.info.enable = 0;
		if (rte_eth_dev_filter_ctrl(0, RTE_ETH_FILTER_HASH, RTE_ETH_FILTER_SET, &info) < 0) {
			printf("Cannot set symmetric hash enable per port on port %u\n", 0);
			return -1;
		}
		flow = rte_flow_create(portid, &attr, pattern, actions, error);
		if (flow == NULL) {
			return -1;
		}
		
	}
	return 0;
}

```

## Copy packet offset to mbuf <a name="copyoffset"></a>
Normally `mbuf->data_off  = RTE_PKTMBUF_HEADROOM` which is the offset to the beginnig of packet data. 
When enabling *copy packet offset to mbuf*, a predefined packet offset is copied into `mbuf->data_off` replacing
the offset to the beginning of packet data.

Supported offsets are:

| Offset in packet data |
|----------------------------|
|`StartOfFrame`             |
|`Layer2Header`             |
|`FirstVLAN`                  |
|`FirstMPLS`                   |
|`Layer3Header`            |
|`Layer4Header`            |
|`Layer4Payload`           |
|`InnerLayer2Header`   |
|`InnerFirstVLAN`          |
|`InnerFirstMPLS`          |
|`InnerLayer3Header`   |
|`InnerLayer4Header`   |
|`InnerLayer4Payload`  |
|`EndOfFrame`             |

To enable *copy packet offset to mbuf*, set following in common_base

- `CONFIG_RTE_LIBRTE_PMD_NTACC_COPY_OFFSET=y`
- `CONFIG_RTE_LIBRTE_PMD_NTACC_OFFSET0=InnerLayer3Header`

This setting will enable copying of offset to the inner layer3 header to `mbuf->data_off`, so 
`mbuf->data_off  = RTE_PKTMBUF_HEADROOM + "offset to  InnerLayer3Header"`

To access the offset, use the command:
```
struct ipv4_hdr *pHdr = (struct ipv4_hdr *)&(((uint8_t *)mbuf->buf_addr)[mbuf->data_off]);
```

Other offsets can be chosen by setting `CONFIG_RTE_LIBRTE_PMD_NTACC_OFFSET0` to the wanted offset.

> Note: If a packet does not contain the wanted layer, the offset is undefined. Due to the filter setup, this will normally never happen.

## Use NTPL filters addition (Making an ethernet over MPLS filter) <a name="ntplfilter"></a>
By using the `RTE_FLOW_ITEM_TYPE_NTPL` rte_flow item, it is possible to use some NTPL commands and thereby create filters that are not a part of the DPDK.

The 	`RTE_FLOW_ITEM_TYPE_NTPL` struct:

```
enum {
	RTE_FLOW_NTPL_NO_TUNNEL,
	RTE_FLOW_NTPL_TUNNEL,
};

struct rte_flow_item_ntpl {
	const char *ntpl_str;
	int tunnel;
};
```
#### ntpl_str
The string that will be embedded into the resulting NTPL filter expression. Ensure that the string will not break the NTPL filter expression. See below for an example.

Following RTE_FLOW filter without the `RTE_FLOW_ITEM_TYPE_NTPL`:
```
ipv4_spec.hdr.src_addr = rte_cpu_to_be_32(IPv4(10,0,0,1));
ipv4_spec.hdr.dst_addr = rte_cpu_to_be_32(IPv4(159,20,6,6));
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
pattern[patternCount].spec = &ipv4_spec;
patternCount++;

pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
patternCount++;
```
Will be converted to following NTPL filter:
```
KeyType[name=KT3;Access=partial;Bank=0;colorinfo=true;tag=port0]={64}
KeyDef[name=KDEF3;KeyType=KT3;tag=port0]=(Layer3Header[12]/64)
KeyList[KeySet=3;KeyType=KT3;color=305419896;tag=port0]=(0x0A0000019F140606)
assign[priority=1;Descriptor=DYN3,length=22,colorbits=32;streamid=0;tag=port0]=(Layer3Protocol==IPV4) and port==0 and Key(KDEF3)==3
```
Adding `RTE_FLOW_ITEM_TYPE_NTPL` to the RTE_FLOW filter:
```
ntpl_spec.ntpl_str = "TunnelType==EoMPLS";
ntpl_spec.tunnel = RTE_FLOW_NTPL_TUNNEL;
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_NTPL;
pattern[patternCount].spec = &ntpl_spec;
patternCount++;

ipv4_spec.hdr.src_addr = rte_cpu_to_be_32(IPv4(10,0,0,1));
ipv4_spec.hdr.dst_addr = rte_cpu_to_be_32(IPv4(159,20,6,6));
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
pattern[patternCount].spec = &ipv4_spec;
patternCount++;

pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
patternCount++;
```
Will be converted to following NTPL filter:
```
KeyType[name=KT3;Access=partial;Bank=0;colorinfo=true;tag=port0]={64}
KeyDef[name=KDEF3;KeyType=KT3;tag=port0]=(InnerLayer3Header[12]/64)
KeyList[KeySet=3;KeyType=KT3;color=305419896;tag=port0]=(0x0A0000019F140606)
assign[priority=1;Descriptor=DYN3,length=22,colorbits=32;streamid=0;tag=port0]=(InnerLayer3Protocol==IPV4) and TunnelType==EoMPLS and port==0 and Key(KDEF3)==3
```
The string "TunnelType==EoMPLS" is now embedded into to the NTPL filter expression, and the resulting filter is changed to a ethernet over MPLS filter matching the inner layer3 protocol.
 
#### tunnel
This tells the driver whether the following rte_flow filter items should be inner or outer filter items

In the above example, tunnel=RTE_FLOW_NTPL_TUNNEL, which makes the filter an inner layer3 filter. If tunnel=RTE_FLOW_NTPL_NO_TUNNEL, the filter is now a outer layer3 filter.
```
KeyType[name=KT3;Access=partial;Bank=0;colorinfo=true;tag=port0]={64}
KeyDef[name=KDEF3;KeyType=KT3;tag=port0]=(Layer3Header[12]/64)
KeyList[KeySet=3;KeyType=KT3;color=305419896;tag=port0]=(0x0A0000019F140606)
assign[priority=1;Descriptor=DYN3,length=22,colorbits=32;streamid=0;tag=port0]=(Layer3Protocol==IPV4) and TunnelType==EoMPLS and port==0 and Key(KDEF3)==3
```
`InnerLayer3Header` and `InnerLayer3Protocol` is now changed to `Layer3Header` and `Layer3Protocol`. 
   
Other NTPL filter expressions can be used as long as it does not break the resulting NTPL filter expression.


## Batching - Receive a batch of packets<a name="batching"></a>
Barching is the possibility to receive a batch of packets instead of one packet at a time. The advantage of using batching is that packets are DMA'ed directly from the adapter in to the batching buffer and thereby no copying is done (zero copy). The disadvantage by using batching is that packets must not be kept for later analysis. If a packet needs to be kept, it must be copied to another buffer. It can be done by using a helper function ([see description below](#helperfunc)). A batch of packets are released when a new batch is requested. The packet data in the previous batch will then be invalid.

> Note: Batching is a Napatech addition to DPDK and is not compatible with any standard applications. Some small changes must be done to implement batching. See the batching example for how to use batching.

### mbuf changes<a name="mbuf"></a>
In order to support batching in DPDK, a some mbuf changes have been made.

A new packet offload features flag `PKT_BATCH` has been added. If this flag is set it means that the mbuf contains a batching buffer and needs special handling.

```
if (mbuf->ol_flags & PKT_BATCH) {
  // mbuf contains a batching buffer
}
else {
  // mbuf contains a standard DPDK packet
}
```

When a mbuf that contains a batch buffer is received the following mbuf variables change their function and the following new mbuf variables are used.

| mbuf changes (batch) | Description |   |
|------------------|----------------|----|
|mbuf->buf_addr | Contains a pointer to the batch buffer | New function|
| mbuf->batch_nb_packet | The number of packets in a batch | Addition |
| mbuf->data_off | Not used. Always 0 |
| mbuf->data_len | Length of the first packet in the batch buffer | New function|
| mbuf->pkt_len | Length of the batch buffer | New function|
| mbuf->userdata | Pointer to the batch control buffer<br>Must not be changed | New function|
| mbuf->port | DPDK port number |
| mbuf->batch_release_cb | Pointer to callback function called when the batch buffer is released<br>Must not be changed | Addition |
  

### Batching buffer<a name="batchbuf"></a>
The batching buffer is different from the standard mbuf packet buffer as it contains a batch of packets and each packet contain a packet descriptor. When a mbuf that contains a batch buffer is received the `mbuf->buf_addr` variable will point to the beginning of the batch buffer. The number of packets in the batch buffer are given by the variable `mbuf->batch_nb_packet`. The packet descriptor that is placed in front of each packet is shown below.

The packet descriptor:
```
struct rte_mbuf_batch_pkt_hdr  {
  uint64_t storedLength:14; /**< The length of the packet incl. descriptor. */ /*  0*/
  uint64_t wireLength:14;   /**< The wire length of the packet.             */ /* 14*/
  uint64_t color_lo:14;     /**< Programmable packet color[13:0].           */ /* 28*/
  uint64_t rxPort:6;        /**< The port that received the frame.          */ /* 42*/
  uint64_t descrFormat:8;   /**< The descriptor type.                       */ /* 48*/
  uint64_t descrLength:6;   /**< The length of the descriptor in bytes.     */ /* 56*/
  uint64_t tsColor:1;       /**< Timestamp color.                           */ /* 62*/
  uint64_t ntDynDescr:1;    /**< Set to 1 to identify this descriptor as a  */ /* 63*/
                            /**< dynamic descriptor.                        */
  uint64_t timestamp;       /**< The time of arrival of the packet.         */ /* 64*/
  uint64_t color_hi:28;     /**< Programmable packet color[41:14].          */ /*128*/
  uint16_t offset0:10;      /**< Programmable offset into the packet.       */ /*156*/
  uint16_t offset1:10;      /**< Programmable offset into the packet.       */ /*166*/
} __attribute__((__packed__)); // descrLength = 22
```

The batch buffer can be browsed in two ways:
- [Directly](#browbatchbuf)
- [Using helper function](#browhelper)

### Browsing the batch buffer directly<a name="browbatchbuf"></a>
The batch buffer can be browsed directly using the packet descriptor to walk through the buffer. A description of the different descriptor variables used are described below.
 
| Variable | Description |
|-----------|-----------|
|mbuf->buf_addr | Pointer to the beginning of the batch buffer |
|phdr->storedLength | The length of the packet including the descriptor |
| phdr->wireLength | The length of the packet on the wire |
|phdr->rxPort | The port number, the packet is received on.<br>Note: The port number is the local port number of the adapter<br>It is not the DPDK port number. |
| phdr->descrLength | The length of the descriptor |
| phdr->timestamp | Packet timestamp |
| phdr->offset0 | Offset to the layer3 header.<br>Note: Can be changed in common_base by setting<br>[`CONFIG_RTE_LIBRTE_PMD_NTACC_COPY_OFFSET`](#copyoffset) and<br>[`CONFIG_RTE_LIBRTE_PMD_NTACC_OFFSET0`](#copyoffset) |
| phdr->color_hi | Packet hash value<br>if (phdr->descrLength == 20) |
| phdr->color_hi<br>phdr->color_lo | Packet MARK value<br>if (phdr->descrLength == 22)<br>MARK = ((phdr->color_hi << 14) & 0xFFFFC000) | phdr->color_lo |

An example of how to browse the batching buffer.

```
struct rte_mbuf_batch_pkt_hdr *phdr; 

phdr = mbuf->buf_addr;  // Point to the beginning of the batch buffer
for (pack = 0; pack < mbuf->batch_nb_packet; pack++) {
  countOctets += phdr->wireLength;
  countPakets++;

  // Dump the IPV4 addresses
  #define IPV4_ADDRESS(a) ((const char *)&a)[0] & 0xFF, \
                          ((const char *)&a)[1] & 0xFF, \
                          ((const char *)&a)[2] & 0xFF, \
                          ((const char *)&a)[3] & 0xFF

  // Point to Layer3 using the data offset
  struct ipv4_hdr *ipv4hdr = (struct ipv4_hdr *)((uint8_t *)phdr + phdr->descrLength + phdr->offset0);
  printf("Src IP: %u.%u.%u.%u - Dst IP: %u.%u.%u.%u\n", IPV4_ADDRESS(ipv4hdr->src_addr),
                                                        IPV4_ADDRESS(ipv4hdr->dst_addr));
  // Point to next packet
  phdr = (struct rte_mbuf_batch_pkt_hdr *)((u_char *)phdr + phdr->storedLength);
}
```
> Note: It is not allowed to keep packets for further analysis. The packets will be invalid when a new batch buffer is requested. To keep a packet, it must be copied to a local mbuf using the function [`rte_pktmbuf_batch_copy_packet_from_batch`](#copybatch)

### Browsing the batch buffer using mbuf helper function<a name="browhelper"></a>
The batch buffer can be browsed using a helper function [`rte_pktmbuf_batch_get_next_packet`](#browhelper). The helper function returns packet data in a mbuf. A description of the different mbuf variables used are described below.

| Variable | Description |
|-----------|-----------|
| m.buf_addr | Pointer to the packet including the packet descriptor |
| m.port  | The port number, the packet is received on.<br>Note: The port number is the local port number of the adapter<br>It is not the DPDK port number. |
| m.data_len | The length of the packet on the wire |
| m.pkt_len | The captured length of the packet incl. the packer descriptor |
| m.data_off | Offset to the layer2 header. After the packet descriptor |
| m.hash.rss | HASH value of the packet.<br>if (m->ol_flags & PKT_RX_RSS_HASH) |
| m.hash.fdir.hi | The MARK value.<br>if (m->ol_flags & (PKT_RX_FDIR_ID | PKT_RX_FDIR)) |
| m.timestamp | Packet timestamp.<br>if (m->ol_flags & PKT_RX_TIMESTAMP) |

An example of how to browse the batching buffer using helper function.

```
uint32_t offset;       // Offset in batching buffer
struct rte_mbuf m1;    // mbuf to hold a single packet.

offset = 0; // Set to 0 to indicate the first packet
for (pack = 0; pack < mbuf->batch_nb_packet; pack++) {
  rte_pktmbuf_batch_get_next_packet(mbuf, &m1, &offset); // Copy pointers and info to the mbuf.

  countOctets += m1.data_len;   // This is the wirelength
  countPakets++;
                                                    
  // Dump the IPV4 addresses
  #define IPV4_ADDRESS(a) ((const char *)&a)[0] & 0xFF, \
                          ((const char *)&a)[1] & 0xFF, \
                          ((const char *)&a)[2] & 0xFF, \
                          ((const char *)&a)[3] & 0xFF

  // Point to Layer3 using the data offset
  struct ipv4_hdr *ipv4hdr = (struct ipv4_hdr *)((uint8_t *)m.buf_addr + m.data_off + ETHER_HDR_LEN); 
  printf("Src IP: %u.%u.%u.%u - Dst IP: %u.%u.%u.%u\n", IPV4_ADDRESS(ipv4hdr->src_addr),
                                                        IPV4_ADDRESS(ipv4hdr->dst_addr));
}
```
> Note: It is not allowed to keep packets for further analysis. The packets will be invalid when a new batch buffer is requested. To keep a packet, it must be copied to a local mbuf using the function [`rte_pktmbuf_batch_copy_packet_from_mbuf`](#copymbuf)

### Helper functions<a name="helperfunc"></a>

#### rte_pktmbuf_batch_get_next_packet - Browse the batch buffer<a name="getnext"></a>
To get the next packet from the batch buffer in a mbuf. The packets are still placed in the batch buffer. No copying has been done. The mbuf->buf_addr points to the packet in the batch buffer.

```
/**
 * Get the next packet from the batching buffer.
 *
 * This function will return the next packet from a batching
 * buffer in a local mbuf.
 *
 * @param mbuf
 *   m_batch:     mbuf containing a batching buffer
 *   m:         local mbuf. Packet data is return in this mbuf.
 *                          Note: No packet data is copied.
 *   offset:    Offset in the batch buffer
 * @return
 *   Number of bytes in returned packet incl. packet descriptor
 */
static inline int
rte_pktmbuf_batch_get_next_packet(struct rte_mbuf *m_batch, 
                                  struct rte_mbuf *m, 
                                  uint32_t *offset)
```

#### rte_pktmbuf_batch_copy_packet_from_batch - Copy a packet from the batch buffer<a name="copybatch"></a>
Copy a packet from the batch buffer using the packet descriptor pointer into a new allocated mbuf.
If the packets is larger than the mbuf buffer size, the packet will be copied into several new allocated linked mbufs.

The returned mbuf must be kept for later analysis, It must be freed after it is used.

```
/**
 * Copy a packet from a batching buffer to a normal mbuf.
 *
 * This function will copy a packet from a batching buffer 
 * to a normal mbuf. The function will allocate the needed number of 
 * mbufs to hold the packet. 
 *
 * @param mbuf
 *   hdr:     Pointer to the batching buffer
 *   mp:    Pointer to memory pool to allocate mbufs from.
 * @return
 *   Pointer to new mbuf or NULL if it fails
 */
static inline struct rte_mbuf *
rte_pktmbuf_batch_copy_packet_from_batch(struct rte_mbuf_batch_pkt_hdr *hdr,
                                         struct rte_mempool *mp)
```

An example of how to use the copy packet function.
```
phdr = mbuf->buf_addr;  // Point to the beginning of the batch buffer
for (pack = 0; pack < mbuf->batch_nb_packet; pack++) {
  struct rte_mbuf *next;
  struct rte_mbuf *m = rte_pktmbuf_batch_copy_packet_from_batch(phdr, data->mbuf_pool);
  next = m;

  // If packet length is larger than the mbuf buffer size
  // the mbuf is returned as a number of linked mbufs
  while (next != NULL) {
    countOctets += next->data_len; // The wire length
    next = next->next;
  }
  countPakets++;
  rte_pktmbuf_free(m);

  // Point to next packet
  phdr = (struct rte_mbuf_batch_pkt_hdr *)((u_char *)phdr + phdr->storedLength);
}
```

#### rte_pktmbuf_batch_copy_packet_from_mbuf - Copy a packet from the batch buffer using mbuf<a name="copymbuf"></a>
Similar to the above copy function, but instead of using the packet descriptor pointer, the mbuf returned from the rte_pktmbuf_batch_get_next_packet is used. Otherwise there is no difference.
 
```
/**
 * Copy a packet from a mbuf batching buffer to a normal mbuf.
 *
 * This function will copy a packet from a mbuf batching buffer 
 * to a normal mbuf. The function will allocate the needed number of 
 * mbufs to hold the packet. 
 *
 * @param mbuf
 *   hdr:     Pointer to the batching buffer
 *   mp:    Pointer to memory pool to allocate mbufs from.
 * @return
 *   Pointer to new mbuf or NULL if it fails
 */
static inline struct rte_mbuf *
rte_pktmbuf_batch_copy_packet_from_mbuf(struct rte_mbuf *mbuf,
                                        struct rte_mempool *mp)
```

### Batching example<a name="batchexam"></a>
An example showing how to use batching is placed in the directory:

`examples/batching`

A number of command line option can be used to control the number of ports to receive data from and the number of queues per port (receive side scaling). The example will setup an IPV4 rte_flow filter in the function `SetupFilter`. This can either be a catch all IPV4 data filter or by using the `-d` and/or `-i` command line options a source and/or destination IP address can be added to the filter to catch only certain IPV4 addresses.

The example can either use SW statistics where the received packets are counted in software or HW statistics where  the number of received packets are returned from the adapter.

batching example command line options:
```
./build/app/batching [EAL options] -- [-p no_ports][-q queues_per_port][-t parse_type][-s stat_type][-i ip_addr][-d ip_addr][-e]
  -p no_ports: Number of ports to use. Always starting with port 0.
  -q queues_per_port: Number of queue per port
  -t parse_type: Type of parsing done
                 0: Parse packets directly from the batch buffer
                 1: Parse packets using helper function
  -s stat_type:  Type of statistic used
                 0: Use hardware statistics
                 1: Use software statistics
  -i ip_addr:    Source IP address to use in filter
  -d ip_addr:    Destination IP address to use in filter
  -e:            Fail if non batching segments are received

  lcores used are equal to no_ports * queues_per_port + 1
```    
> Note: Using the option -e causing the batching example to fail if a non batch mbuf is received.

Output of the batching example:
```
./build/app/batching -c 1f -- -q 4 -p 1 -t 0 -e
EAL: Detected 32 lcore(s)
EAL: Probing VFIO support...
EAL: PCI device 0000:02:00.0 on NUMA socket 0
EAL:   probe driver: 18f4:1a5 net_ntacc
EAL: PCI device 0000:03:00.0 on NUMA socket 0
EAL:   probe driver: 18f4:1a5 net_ntacc
PMD: Checking: 0000:03:00.0
PMD: Adapter not found
EAL: PCI device 0000:06:00.0 on NUMA socket 0
EAL:   probe driver: 18f4:165 net_ntacc
PMD: Checking: 0000:06:00.0
PMD: Adapter not found
EAL: PCI device 0000:07:00.0 on NUMA socket 0
EAL:   probe driver: 18f4:165 net_ntacc
EAL: PCI device 0000:09:00.0 on NUMA socket 0
EAL:   probe driver: 8086:1533 net_e1000_igb
EAL: PCI device 0000:0a:00.0 on NUMA socket 0
EAL:   probe driver: 8086:1533 net_e1000_igb
EAL: PCI device 0000:81:00.0 on NUMA socket 1
EAL:   probe driver: 8086:10fb net_ixgbe
EAL: PCI device 0000:81:00.1 on NUMA socket 1
EAL:   probe driver: 8086:10fb net_ixgbe
EAL: PCI device 0000:82:00.0 on NUMA socket 1
EAL:   probe driver: 18f4:145 net_ntacc
PMD: Checking: 0000:82:00.0
PMD: Found: 0000:82:00.0: Ports 4, Offset 0, Adapter 0
PMD: Port: 0 - 0000:82:00.0 Port 0
PMD: Tagname: port0 - 0
PMD: Port: 1 - 0000:82:00.0 Port 1
PMD: Tagname: port1 - 1
PMD: Port: 2 - 0000:82:00.0 Port 2
PMD: Tagname: port2 - 2
PMD: Port: 3 - 0000:82:00.0 Port 3
PMD: Tagname: port3 - 3
Port 0 MAC: 00 0d e9 03 bd ce
Core 1 capturing packets for port 0, Queue 0. Parsing done directly in batching buffer
Non batching queue not accepted.
Core 2 capturing packets for port 0, Queue 1. Parsing done directly in batching buffer
Non batching queue not accepted.
Core 3 capturing packets for port 0, Queue 2. Parsing done directly in batching buffer
Non batching queue not accepted.
Core 4 capturing packets for port 0, Queue 3. Parsing done directly in batching buffer
Non batching queue not accepted.
Core 0 is handling software statistics output

Q0,0: 0 pk,     0.0 Mbps Q0,1: 0 pk,     0.0 Mbps Q0,2: 0 pk,     0.0 Mbps Q0,3: 0 pk,     0.0 Mbps

^C

Port 0 - Queue 0: Received                0 pkts
Port 0 - Queue 1: Received                0 pkts
Port 0 - Queue 2: Received                0 pkts
Port 0 - Queue 3: Received                0 pkts
```
The output are:

QX,Y: 0 pk,     0.0 Mbps 
- X = The port packets is received on. 
- Y = The queue on port X containing the packets.
- Z pk = Number of packets received.
- 0.0 Mbps = RX speed 
