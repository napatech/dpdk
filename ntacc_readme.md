# Napatech Virtual Poll Mode driver – NTACC PMD.
----------
The Napatech NTACC PMD enables users to run DPDK on top of the Napatech accelerators and driver. The NTACC PMD is a virtual driver and must be configured and started using the "--vdev" configuration parameter applied to DPDK when starting applications using DPDK.



> Note: As the NTACC PMD driver is virtual, the dpdk-devbind-py script cannot be used to bind the interface.


## Napatech Driver ##

The Napatech driver and adapter must be installed and started before the NTACC PMD can be used. See the installation guide in the Napatech driver package for how to install and start the driver.

See below for supported drivers and adapters:

|  Supported drivers |
|-------------------------|
| 3.6.6                      |

<br>

|  Supported Adapters                              | FPGA                        |
|---------------------------------------------------|---------------------------|
|  NT40A01-01-SCC-4×1-E3-FF-ANL        |  200-9500-08-06-00 |
|  NT20E3-2-PTP-ANL                               |  200-9501-08-06-00 |
|  NT40E3-4-PTP-ANL                               |  200-9502-08-06-00 |
|  NT80E3-2-PTP-ANL                               |  200-9503-08-06-00 |
|  NT100E3‐1‐PTP‐ANL                             |  200-9505-08-06-00 |
|  NT200A01-02-SCC-2×100-E3-FF-ANL  |  200-9508-07-06-00 |
|  NT200A01-02-SCC-2×40-E3-FF-ANL    |  200-9512-08-08-00 |



## Compiling the Napatech NTACC PMD driver 

##### Environment variable 
In order to compile the NTACC PMD the NAPATECH3_PATH environment variable must be set. This tells DPDK where the Napatech driver is installed.

`export NAPATECH3_PATH=/opt/napatech3`

/opt/napatech3 is the default path for installing the Napatech driver. If the driver is installed elsewhere, that path must be used.

##### Configuration setting
To enable DPDK to compile NTACC PMD a configuration setting must be set in the file common_base.

`CONFIG_RTE_LIBRTE_PMD_NTACC=y`

Two other configurations settings can be used to change the behaviour of the NTACC PMD:

- Hardware based or software based statistic:
This setting is used to select between software based and hardware based statistic.
`CONFIG_RTE_LIBRTE_PMD_NTACC_USE_SW_STAT=n`

- Disable default filter:
This setting is used to disable generation of a default catch all filter. See [Default filter](#default-filter) for further information.
`CONFIG_RTE_LIBRTE_PMD_NTACC_DISABLE_DEFAULT_FILTER=n`

## Napatech Driver Configuration
The Napatech driver is configured using an ini-file – `ntservice.ini`. By default, the ini-file is located in `/opt/napatech3/config`. The following changes must be made to the default ini-file.

The Napatech driver uses hostbuffers to receive and transmit data. The number of hostbuffers must be equal to or larger than the number of RX and TX queues used by the DPDK.

To change the number of hostbuffers, edit ntservice.ini and change following keys:

	HostBuffersRx = [R1, R2, R3]
	HostBuffersTx = [T1, T2, T3]

Hostbuffer settings:

| Parameter | Description                                  |                                                                                                                                          |
|-----------|----------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| R1        |  Number of RX hostbuffers                    | Must be equal to or larger than the number of RX queues used.<br>Maximum value = 128.                                                    | 
| R2        |  Size of a hostbuffer in MB                  | The optimal size depends of the use. The default is usually fine.<br>The value must be a multiple of 4 and larger or equal than 16.      |
| R3        |  Numa node where the hostbuffer is allocated | Use -1 for NUMA node autodetect. Hostbuffers will be allocated from the NUMA node to which the accelerator is connected.                 |
| T1        |  Number of TX hostbuffers                    | Must be equal to or larger than the number of TX queues used.<br>Maximum value = 128.                                                    | 
| T2        |  Size of a hostbuffer in MB                  | The optimal size depends of the use. The default is usually fine.<br>The value must be a multiple of 4 and larger or equal than 16.      |
| T3        |  Numa node where the hostbuffer is allocated | Use -1 for NUMA node autodetect. Hostbuffers will be allocated from the NUMA node to which the accelerator is connected.                 |

The default setting is:

```
HostBuffersRx = [4, 16, -1]
HostBuffersTx = [4, 16, -1]
```
This means that it would be possible to create 4 RX queues and 4 TX queues.

## Number of RX queues and TX queues available

Up to 128 RX queues are supported. They are distributed between the ports on the Napatech accelerator and rte_flow filters on a first-come, first-served basis.

Up to 128 TX queues are supported. They are distributed between the ports on the Napatech accelerator on a first-come, first-served basis.

The maximum number of RX queues per port are the smallest number of either:

- (256 / number of ports)
- `RTE_ETHDEV_QUEUE_STAT_CNTRS`

`RTE_ETHDEV_QUEUE_STAT_CNTRS` is defined in `common_config`

## Starting NTACC PMD

The NTACC PMD is a virtual driver. To start the driver, the DPDK `--vdev` option must be used.

The `--vdev` option is also used to select the port on the Napatech accelerator to be used.

| --vdev parameter    | Description                                   |
|---------------------|-----------------------------------------------|
| `<driver><id>`	  |Name of the driver followed by an ID.          |
|`port=<port number>` |Port number of the Napatech accelerator to use.|

Examples:
- `--vdev eth_ntacc0, port=3` 
DPDK port 0 mapped to accelerator port 3

- `--vdev eth_ntacc0,port=0 --vdev eth_ntacc1,port=1 --vdev eth_ntacc2,port=2 --vdev eth_ntacc3,port=3`
DPDK port 0, 1, 2, 3 mapped to accelerator port 0, 1, 2, 3

- `--vdev eth_ntacc2,port=2 --vdev eth_ntacc1,port=1`
DPDK port 0, 1 mapped to accelerator port 2, 1

The DPDK ports are numbered in the order they are listed, so the first `--vdev` will be port number 0 and so on.
This makes it possible to map the DPDK port to a arbitrary port on the Napatech accelerator.
 
## Generic rte_flow filter items

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

The following rte_flow filters are added by Napatech and are not a part of the main DPDK:

| rte_flow filter	         | Supported fields           | 
|----------------------------|----------------------------|
|`RTE_FLOW_ITEM_TYPE_GREv0`    | Only packet type = `GREv0`   |
|`RTE_FLOW_ITEM_TYPE_GREv1`    | Only packet type = `GREv1`   |
|`RTE_FLOW_ITEM_TYPE_GTPv1_U`  | Only packet type = `GTPv1_U` |
|`RTE_FLOW_ITEM_TYPE_GTPv0_U`  | Only packet type = `GTPv0_U` |
|`RTE_FLOW_ITEM_TYPE_IPinIP`   | Only packet type = `IPinIP`  |

**Supported fields**: The fields in the different filters that can be used when creating a rte_flow filter.

**Only packet type**: No fields can be used. The filter will match packets containing this type of tunnel.

## An example of an inner (tunnel) filter

All flow items defined after a tunnel flow item will be an inner filter, as shown below:

1. `RTE_FLOW_ITEM_TYPE_ETH`: Outer ether filter
2. `RTE_FLOW_ITEM_TYPE_IPV4`: Outer IPv4 filter
3. `RTE_FLOW_ITEM_TYPE_NVGRE`: Tunnel filter
4. `RTE_FLOW_ITEM_TYPE_ETH`: Inner ether filter (because of the tunnel filter)
5. `RTE_FLOW_ITEM_TYPE_IPV4`: Inner IPv4 filter (because of the tunnel filter)

## Generic rte_flow filter Attributes

The following rte_flow filter attributes are supported:

| Attribute     | Values                                                 |
|---------------|--------------------------------------------------------|
|`priority`     | 0 – 62<br>Highest priority = 0<br>Lowest priority = 62 |
|`ingress`      |                                                        |

## Priority
If multiple filters are used, priority is used to select the order of the filters. The filter with the highest priority will always be the filter to be used. If filters overlap, for example an ethernet filter sending the packets to queue 0 and an IPv4 filter sending the packets to queue 1 (filters overlap as IPv4 packets are also ethernet packets), then the filter with the highest priority is used. 

If the ethernet filter has the highest priority all packets will go to queue 0 and no packets will go to queue 1.

If the IPv4 filter has the highest priority all IPv4 packets will go to queue 1 and all other packets will go to queue 0.

If the filters have the samme priority, the filter entered last is the one to be used.

## Generic rte_flow filter actions

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
  - If a packet not matching the filter is received the HASH value of the packet will be copied to mbuf->hash.rss and the PKT_RX_RSS_HASH flag in mbuf->ol_flags is set.

Note: Currently maximum MARK value is 0x1FFF (13 bit).

- `RTE_FLOW_ACTION_TYPE_RSS`
The supported HASH function is described below.

## Generic rte_flow RSS/Hash functions

Following rte_flow filter HASH functions are supported:

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

##### Symmetric/Unsymmetric hash
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
 


##### Default RSS/HASH function
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


## Default filter
When starting the NTACC PMD driver, a default catch all filter with priority 62 (lowest priority) is created for each DPDK port. The filter will send all incoming packets to queue 0 for each DPDK port. If rte_flow filters are created with higher priority, then all packets matching these filters will be send to the queues defined by the filters. All packets not matching the filter will be send to queue 0.

If a default RSS (hash) mode is defined using the rte_eth_dev_configure command (mq_mode = ETH_MQ_RX_RSS), a default catch all filter is created, that will send incoming packet to all defined queues using the defined RSS/HASH function. 

> With a default RSS/HASH function, the default filter will collide with any rte_flow filters created, as all non matched packets will be distributed to all defined queues using the default RSS/HASH function. It is recommended not to define a default RSS/HASH function if any rte_flow filters are going to be used.

If a default filter is not wanted, it can be disabled by setting:

`CONFIG_RTE_LIBRTE_PMD_NTACC_DISABLE_DEFAULT_FILTER=y`

## Examples of generic rte_flow filters

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

An ether outer ether filter and an IPV4 inner filter:
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

All rte_flow filters added in same rte_flow_create will be and’ed together. If any filter have to be created where the it needs to be or’ed together, it has to be done with several calls to rte_flow_create for each filter.

## Limited filter resources

The Napatech accelerator and driver has a limited number of filter resources, when using the generic rte_flow filter. In some cases a filter cannot be created. In these cases, it will be necessary to simplify the filter.

## Filter creation example
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

