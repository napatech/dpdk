# Napatech PCI Poll Mode Driver – NTACC PMD.
----------
The Napatech NTACC PMD enables users to run DPDK on top of the Napatech SmartNics and driver. The NTACC PMD is a PCI driver.

The NTACC PMD driver does not need to be bound. This means that the dpdk-devbind-py script cannot be used to bind the interface. The DPDK app will automatically find and use the NTACC PMD driver when starting, provided that the Napatech driver is started and the Napatech SmartNic is not blacklisted.

## Table of Contents
1. [Napatech Driver](#driver)
	1. [SmartNic with Limited filter support](#LimitedFilter)
2. [Compiling the Napatech NTACC PMD driver](#compiling)
	1. [Environment variable](#Environment)
	2. [Configuration setting using meson/ninja](#configurationmeson)
3. [Napatech Driver Configuration](#driverconfig)
	1. [Statistics update interval](#statinterval)
4. [Number of RX queues and TX queues available](#queues)
5. [Starting NTACC PMD](#starting)
6. [DPDK Sample applications supported by the Napatech adapter](#sampleapps)
7. [Priority](#Priority)
8. [Generic rte_flow filter items](#genericflow)
9. [Generic rte_flow RSS/Hash functions](#hash)
10. [Generic rte_flow filter Attributes](#attributes)
11. [Generic rte_flow filter actions](#actions)
12. [Generic rte_flow RSS/Hash functions](#hash)
	1. [Symmetric/Unsymmetric hash](#Symmetric)
	2. [Default RSS/HASH function](#DefaultRSS)
13. [Catch All Filter - promiscuous mode](#DefaultFilter)
	1. [Disabling catch all filter](#DisablingDefaultFilter)
14. [Examples of generic rte_flow filters](#examples1)
15. [Limited filter resources](#resources)
16. [Filter creation example -  5tuple filter](#Filtercreationexample)
17. [Filter creation example - Multiple 5tuple filter (IPv4 addresses and TCP ports)](#examples2)
18. [Copy packet offset to mbuf](#copyoffset)
19. [Use NTPL filters addition (Making an ethernet over MPLS filter)](#ntplfilter)
20. [Packet decoding offload](#hwdecode)
	1. [Layer3 and Layer4 packet decoding offload](#hwl3l4)
	2. [Inner most Layer3 and Layer4 packet decoding offload](#hwil3il4)
	3. [More packet decoding types](#hwmoredecode) 
21. [Use External Buffers (Zero copy)](#externalbuffer)
	1. [Limitation using external buffers](#limitexternalbuffer)
	2. [Enable external buffers](#enableexternalbuffer)
	3. [Using/detecting external buffers](#detectexternalbuffer)

## Napatech Driver <a name="driver"></a>

The Napatech driver and SmartNic must be installed and started before the NTACC PMD can be used. See the installation guide in the Napatech driver package for how to install and start the driver.

> Note: The driver must be the supported version or a newer version.


| Supported driver  |
|-------------------|
| 3.11.0 or newer   |

<br>
All Napatech 4Generation SmartNics are supported. Some SmartNics have limited filter support.

|  Supported SmartNics                        |
|---------------------------------------------|
|Intel® Programmable Acceleration Card with Intel® Arria® 10 GX FPGA|
|NT200A02 SmartNICs (4GA)|
|NT200A01 SmartNICs (4GA)|
|NT200C01 SmartNICs (4GA)|
|NT100A01 SmartNICs (4GA)|
|NT50B01 SmartNICs (4GA)|
|NT100E3-1-PTP SmartNICs (4GA)|
|NT80E3-2-PTP SmartNICs (4GA)|
|NT80E3-2-PTP-8×10/2×40 SmartNICs (4GA)|
|NT40A01 SmartNICs (4GA)|
|NT40E3-4-PTP SmartNICs (4GA)|
|NT20E3-2-PTP SmartNICs (4GA)|

The complete driver package can be downloaded here:
[Link™ Capture Software 12.6.4 for Linux](https://supportportal.napatech.com/index.php?/selfhelp/view-article/link--capture-software-1264-for-linux/646)

#### SmartNic with Limited filter support <a name="LimitedFilter"></a>

| SmartNics | FPGA                        |
|---------------|---------------------------|
|NT200A01  | 200-9516-XX-XX-XX |
|NT200A02  | 200-9534-XX-XX-XX |
|NT200A02  | 200-9535-XX-XX-XX |

When using a SmartNIC with limited filter support only a limited number of rte_flow filters is supported.
The rte_flow filters must not contain any spec, mask or last values like below:
```
memset(&pattern, 0, sizeof(pattern));
uint32_t patternCount = 0;
pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
pattern[patternCount].spec = NULL;
pattern[patternCount].mask = NULL;
pattern[patternCount].last = NULL;
patternCount++;

pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
patternCount++;
```

All item types are supported as well as all actions.

## Compiling the Napatech NTACC PMD Driver <a name="compiling"></a>

##### Environment variable <a name="Environment"></a>
In order to compile the NTACC PMD, the NAPATECH3_PATH environment variable must be set. This tells DPDK where the Napatech driver is installed.

`export NAPATECH3_PATH=/opt/napatech3`

/opt/napatech3 is the default path for installing the Napatech driver. If the driver is installed elsewhere, that path must be used.

##### Configuration setting using meson/ninja <a name="configurationmeson"></a>
The NTACC PMD is automatically compiled.

- Hardware-based or software-based statistic:
This setting is used to select between software-based and hardware-based statistics. To enable the setting run:
<br>`meson setup --reconfigure -Dntacc_use_sw_stat=true`

## Napatech Driver Configuration <a name="driverconfig"></a>
The Napatech driver is configured using an ini-file – `ntservice.ini`. By default, the ini-file is located in `/opt/napatech3/config`. The following changes must be made to the default ini-file.

The Napatech driver uses host buffers to receive and transmit data. The number of host buffers must be equal to or larger than the number of RX and TX queues used by the DPDK.

To change the number of host buffers, edit ntservice.ini and change following keys:

	HostBuffersRx = [R1, R2, R3]
	HostBuffersTx = [T1, T2, T3]

host buffer settings:

| Parameter | Description                                  |                                                                                                                                          |
|-----------|----------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| R1        |  Number of RX host buffers                    | Must be equal to or larger than the number of RX queues used.<br>Maximum value = 128.                                                    |
| R2        |  Size of a host buffer in MB                  | The optimal size depends of the use. The default is usually fine.<br>The value must be a multiple of 4 and larger than or equal to 16.        |
| R3        |  Numa node where the host buffer is allocated | Use -1 for NUMA node autodetect. host buffers will be allocated from the NUMA node to which the SmartNic is connected.                 |
| T1        |  Number of TX host buffers                    | Must be equal to or larger than the number of TX queues used.<br>Maximum value = 128.                                                    |
| T2        |  Size of a host buffer in MB                  | The optimal size depends on the use. The default is usually fine.<br>The value must be a multiple of 4 and larger or equal to 16.        |
| T3        |  Numa node where the host buffer is allocated | Use -1 for NUMA node autodetect. host buffers will be allocated from the NUMA node to which the SmartNic is connected.                 |

The default setting for SmartNic X is:

```
[AdapterX]
HostBuffersRx = [4, 16, -1]
HostBuffersTx = [4, 16, -1]
```
This means that it will be possible to create 4 RX queues and 4 TX queues.

#### Statistics update interval  <a name="statinterval"></a>
When using hardware-based statistics `ntacc_use_sw_stat=true`, the default update interval is 500 ms, i.e. the time between each time the statistics are updated by the SmartNic. In some cases, the update interval is too large. The update interval can be changed by changing the `StatInterval` option in the system section in the ini-file.
```
[System]
StatInterval=1
```

Possible values in milliseconds for StatInterval are:
`1, 10, 20, 25, 50, 100, 200, 400, 500`

> Note: Increasing the statistics update frequency requires more CPU cycles.

## Number of RX Queues and TX Queues Available <a name="queues"></a>

Up to 128 RX queues are supported. They are distributed between the ports on the Napatech SmartNic and rte_flow filters on a first-come, first-served basis.

Up to 128 TX queues are supported. They are distributed between the ports on the Napatech SmartNic on a first-come, first-served basis.

The maximum number of RX queues per port is the smallest number of either:

- (256 / number of ports)
- `RTE_ETHDEV_QUEUE_STAT_CNTRS`

## Starting NTACC PMD <a name="starting"></a>

When a DPDK app is starting, the NTACC PMD is automatically found and used by the DPDK. All Napatech SmartNics installed and activated will appear in the DPDK app. To use only some of the installed Napatech SmartNics, the whitelist command must be used. The whitelist command is also used to select specific ports on an SmartNic.

| whitelist command format |  Description |
|-----------------------------------|---|
| `-w <[domain:]bus:devid.func>` | Select a specific PCI adapter |
| `-w <[domain:]bus:devid.func>,mask=X` | Select a specific PCI adapter, <br>but use only the ports defined by mask<br>The mask command is specific for Napatech SmartNics |



Example 1:
An NT40E3-4-PTP-ANL Napatech SmartNic is installed. We want to use only port 0 and 1.

- `DPDKApp  -w 0000:82:00.0,mask=3`

Example 2:
Two NT40E3-4-PTP-ANL Napatech SmartNics are installed. We want to use only port 0 and 1 on SmartNic 1 and only port 2 and 3 on SmartNic 2.

- `DPDKApp  -w 0000:82:00.0,mask=3 -w 0000:84:00.0,mask=0xC`

Example 3:
An NT40E3-4-PTP-ANL Napatech SmartNic is installed. We want app1 to use only port 0 and 1, and app2 to use only port 2 and 3. In order to start two DPDK applications, we must share the memory between the two applications using --file-prefix and --socket-mem. Note that both applications will get DPDK port number 0 and 1, even though app2 will use port number 2 and 3 on the SmartNic.

- `DPDKApp1  -w 0000:82:00.0,mask=3 --file-prefix fc0 --socket-mem 1024,1024`
- `DPDKApp2  -w 0000:82:00.0,mask=12 --file-prefix fc1 --socket-mem 1024,1024`

<br>
> Note: When using the whitelist command, all SmartNics to be used must be included.

The Napatech SmartNics can also be disabled by using the blacklist command.

## DPDK Sample applications supported by the Napatech adapter<a name="sampleapps"></a>
In order to test the Napatech SmartNics a limited number of the DPDK sample tools listed below can be used. 

| User guide title | Application name | Example directory |
|------------------|------------------|-------------------|
| Basic Forwarding Sample Application | basicfwd | skeleton |
| RX/TX Callbacks Sample Application | rxtx_callbacks | rxtx_callbacks |
| Basic RTE Flow Filtering Sample Application | flow_filtering | flow_filtering |
| L2 Forwarding Sample Application (in Real and Virtualized Environments) | l2fwd | l2fwd |
| L2 Forwarding Sample Application (in Real and Virtualized Environments) with core load statistics | l2fwd-jobstats | l2fwd-jobstats |
| Packet Ordering Application | packet_ordering | packet_ordering |
| Ethtool Sample Application | Ethtool |

> Note: flow_filtering - When using this sample application,[the default filter](#DefaultFilter) should be disabled.

#### Ethtool Sample Application
The following Ethtool commands are supported by the Napatech SmartNics.

| command | parameter |
|---------|-----------|
| `portstats` | `<port>`  |
| `drvinfo` |           |
| `link`    |           |
| `macaddr` | `<port>`    |
| `mtu`     | `<port>` `<value>` |

 
## Generic rte_flow Filter Items <a name="genericflow"></a>

The NTACC PMD driver supports a number of generic rte_flow filters including some Napatech-defined filters.

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
|`RTE_FLOW_ITEM_TYPE_VLAN`  | `tci`                                                                                                                                                                 |
|`RTE_FLOW_ITEM_TYPE_MPLS`  | `label_tc_s` |
|`RTE_FLOW_ITEM_TYPE_NVGRE` | Only packet type = `NVGRE`                                                                                                                                                    |
|`RTE_FLOW_ITEM_TYPE_VXLAN` | Only packet type = `VXLAN`                                                                                                                                                    |
| `RTE_FLOW_ITEM_TYPE_GRE`  | `c_rsvd0_ver` (only version = bit b0-b2)|
| `RTE_FLOW_ITEM_TYPE_PHY_PORT`  | `index`<br>The port numbers used must be the local port numbers for the SmartNic. <br>For a 4 port SmartNic the port numbers are 0 to 3.|
|`RTE_FLOW_ITEM_TYPE_GRE`    | `c_rsvd0_ver=1`: Packet type = `GREv1`<br>`c_rsvd0_ver=0`: Packet type = `GREv0`   |
|`RTE_FLOW_ITEM_TYPE_GTPU`  | `v_pt_rsv_flags=0`: Packet type = `GTPv0_U`<br>`v_pt_rsv_flags=0x20`: Packet type = `GTPv1_U` |
|`RTE_FLOW_ITEM_TYPE_GTPC`  | `v_pt_rsv_flags=0x20`: Packet type = `GTPv1_C`<br>`v_pt_rsv_flags=0x40`: Packet type = `GTPv2_C`<br>No parameter:  Packet type = `GTPv1_C` or `GTPv2_C` |
| `RTE_FLOW_ITEM_TYPE_MPLS` | Label parameter defined in:<br>`label_tc_s[0]`, `label_tc_s[1]`, `label_tc_s[2]` |
The following rte_flow filters are added by Napatech and are not a part of the main DPDK:

| rte_flow filter	         | Supported fields           |
|----------------------------|----------------------------|
|`RTE_FLOW_ITEM_TYPE_IPinIP` | Only packet type = `IPinIP`  |
|`RTE_FLOW_ITEM_TYPE_TUNNEL` | Match on inner layers |
|`RTE_FLOW_ITEM_TYPE_NTPL`   | [see *Use NTPL filters*](#ntplfilter)  |

**Supported fields**: The fields in the different filters that can be used when creating a rte_flow filter.

**Only packet type**: No fields can be used. The filter will match packets containing this type of tunnel.

## An Example of an Inner (Tunnel) Filter

All flow items defined after a tunnel flow item will be an inner filter, as shown below:

1. `RTE_FLOW_ITEM_TYPE_ETH`: Outer ether filter
2. `RTE_FLOW_ITEM_TYPE_IPV4`: Outer IPv4 filter
3. `RTE_FLOW_ITEM_TYPE_NVGRE`: Tunnel filter
4. `RTE_FLOW_ITEM_TYPE_ETH`: Inner ether filter (because of the tunnel filter)
5. `RTE_FLOW_ITEM_TYPE_IPV4`: Inner IPv4 filter (because of the tunnel filter)

## Generic rte_flow Filter Attributes <a name="attributes"></a>

The following rte_flow filter attributes are supported:

| Attribute     | Values                                                 |
|---------------|--------------------------------------------------------|
|`priority`     | 0 – 62<br>Highest priority = 0<br>Lowest priority = 62 |
|`ingress`      |                                                        |

## Priority <a name="Priority"></a>
If multiple filters are used, priority is used to select the order of the filters. The filter with the highest priority will always be the filter to be used. If filters overlap, for example an ethernet filter sending the packets to queue 0 and an IPv4 filter sending the packets to queue 1 (filters overlap as IPv4 packets are also ethernet packets), the filter with the highest priority is used.

If the ethernet filter has the highest priority, all packets will go to queue 0 and no packets will go to queue 1.

If the IPv4 filter has the highest priority, all IPv4 packets will go to queue 1 and all other packets will go to queue 0.

If the filters have the samme priority, the filter entered last is the one to be used.

## Generic rte_flow Filter Actions <a name="actions"></a>

Following rte_flow filter actions are supported:

| Action                      | Supported fields                        |
|-----------------------------|-----------------------------------------|
|`RTE_FLOW_ACTION_TYPE_END`   |                                         |
|`RTE_FLOW_ACTION_TYPE_VOID`  |                                         |
|`RTE_FLOW_ACTION_TYPE_MARK`  | See description below                   |
|`RTE_FLOW_ACTION_TYPE_RSS`   | `func`<br>`level`<br>`types`<br>`queue_num`<br>`queue`|
|`RTE_FLOW_ACTION_TYPE_QUEUE` | `index`                                 |
|`RTE_FLOW_ACTION_TYPE_DROP`  |                                         |
|`RTE_FLOW_ACTION_TYPE_PORT_ID`| `id`                                   |
|`RTE_FLOW_ACTION_TYPE_PHY_PORT`| `index`                               |

- `RTE_FLOW_ACTION_TYPE_MARK`
  - If MARK is set and a packet matching the filter is received, the mark value will be copied to mbuf->hash.fdir.hi and the PKT_RX_FDIR_ID flag in mbuf->ol_flags is set.
  - If a packet not matching the filter is received, the HASH value of the packet will be copied to mbuf->hash.rss and the PKT_RX_RSS_HASH flag in mbuf->ol_flags is set.

- `RTE_FLOW_ACTION_TYPE_RSS`
The supported HASH functions are described below.
- `RTE_FLOW_ACTION_TYPE_DROP`
All packets matching the filter will be dropped by the SmartNIC (HW). The packets will not be sent to the APP.
- `RTE_FLOW_ACTION_TYPE_PORT_ID`
All packets matching the filter will be retransmitted on the DPDK port `id`. The packets will not be sent to the APP. The DPDK port must be on the same SmartNIC as the RX port.
- `RTE_FLOW_ACTION_TYPE_PHY_PORT`
As `RTE_FLOW_ACTION_TYPE_PORT_ID`, but the port number used must be the physical port number on the SmartNIC.
If a 4 ported SmartNIC is used. The port number must be 0 to 3.

## Generic rte_flow RSS/Hash Functions <a name="hash"></a>

The following rte_flow filter HASH functions are supported:

| HASH function	| HASH Keys |  | 
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

##### Inner and Outer Layer Hashing <a name="InnerOuterHashing"></a>

Inner and Outer layer hashing is controlled by the rss command `level`.

| RSS level	| Layer | 
|------|--|
| 0 | Outer layer |
| 1 | Outer layer |
| 2 | Inner layer |

##### Symmetric/Unsymmetric Hash <a name="Symmetric"></a>
The key generation can either be symmetric (sorted) or unsymmetric (unsorted). The key generation is controled the RSS command `func` 

| RSS func	| Key generation | 
|------|--|
| `RTE_ETH_HASH_FUNCTION_DEFAULT` | Unsymmetric (unsorted) |
| `RTE_ETH_HASH_FUNCTION_SIMPLE_XOR` | Symmetric (sorted) |
| `RTE_ETH_HASH_FUNCTION_TOEPLITZ` | Not supported |

The default key generation is set to Unsymmetric. 

> `sorted` and `unsorted` is Napatech terms for symmetric and unsymmetric.

##### Default RSS/HASH function <a name="DefaultRSS"></a>
A default RSS/HASH function can be set up when configuring the ethernet device by using the function:

	int rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue, uint16_t nb_tx_queue, const struct rte_eth_conf *eth_conf);

Using the parameter `eth_conf` of the type `struct rte_eth_conf`, a default RSS/HASH function can be defined.  

Following two fields of the `struct rte_eth_conf` are supported:

- `rxmode.mq_mode`
- `rx_adv_conf.rss_conf.rss_hf`

To set up a default RSS/HASH function use:

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

See below for information about the consequences of setting a default RSS/HASH function.


## Catch All Filter - promiscuous mode<a name="DefaultFilter"></a>
When starting the NTACC PMD driver, no filter is created and therefor no data is received. In order to start receiving data the adapter must either be put in promiscuous mode or an rte_flow filter must be created.

When putting the adapter in promiscuous mode a catch all filter with priority 62 (lowest priority) is created for the DPDK port. The filter will send all incoming packets to queue 0. If rte_flow filters are created with higher priority, then all packets matching these filters will be sent to the queues defined by the filters. All packets not matching the filter will be sent to queue 0.

If a default RSS (hash) mode is defined using the rte_eth_dev_configure command (mq_mode = ETH_MQ_RX_RSS), a default catch all filter is created, that will send incoming packet to all defined queues using the defined RSS/HASH function.

> With a default RSS/HASH function, the default filter will collide with any rte_flow filters created, as all non-matched packets will be distributed to all defined queues using the default RSS/HASH function. It is recommended not to define a default RSS/HASH function if any rte_flow filters are going to be used.


#### Disabling catch all filter<a name="DisablingDefaultFilter"></a>
The catch all filter can be disabled either by disabling promiscuous mode or by using the rte_flow_isolate command:

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

## Limited Filter Resources <a name="resources"></a>

The Napatech SmartNic and driver has a limited number of filter resources when using the generic rte_flow filter. In some cases, a filter cannot be created. In these cases, it will be necessary to simplify the filter.

## Filter Creation Example <a name="Filtercreationexample"></a>
The following example creates a 5-tuple IPv4/TCP filter. If `nbQueues > 1` RSS/Hashing is made to the number of queues using hash function `ETH_RSS_IPV4`. Symmetric hashing is enabled. Packets are marked with 12.
```C++
static struct rte_flow *SetupFilter(uint8_t portid, uint8_t nbQueues)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[10];
	struct rte_flow_action actions[10];
	struct rte_flow_error error;
	uint32_t patternCount = 0;
	uint32_t actionCount = 0;

  	uint16_t *pQueues = NULL;
	struct rte_flow *flow = NULL;
  	struct rte_flow_action_rss rss;

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
    	pQueues = malloc(sizeof(uint16_t) * nbQueues);
		if (!pQueues) {
			printf("Memory allocation error\n");
			return NULL;
		}

    	rss.types = ETH_RSS_IPV4;                    // IPV4 5tuple hashing
    	rss.func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR; // Set symmetric hashing
    	rss.level = 0;                               // Set outer layer hashing

    	rss.queue_num = nbQueues;
		for (i = 0; i < nbQueues; i++) {
			pQueues[i] = i;
		}
    	rss.queue = pQueues;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
		actions[actionCount].conf = &rss;
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

	flow = rte_flow_create(portid, &attr, pattern, actions, &error);
	if (!flow) {
		printf("Filter error: %s\n", error.message);
		if (pQueues) {
			free(pQueues);
		}
		return NULL;
	}

	if (pQueues) {
		free(pQueues);
	}
	return flow;
}
```

## Filter Creation Example - Multiple 5-tuple Filter (IPv4 Addresses and TCP Ports) <a name="examples2"></a>
The following example shows how it is possible to create a 5-tuple filter matching on a large number of IPv4 addresses and TCP ports.

The commands used in the loop to program the IP addresses and tcp ports must be the same for all addresses and ports. The only things that must be changed are the values for IP addresses and tcp ports.

The driver is then able to optimize the final filter making it possible to have up to 20.000 filter items.

> Note: The number of possible filter items depends on the filter made and will vary depending on the complexity of the filter.

> Note: The way the filter in this example is made can be used for all rte_flow_items as long as only the filter values are changed.

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

static int SetupFilterxxx(uint8_t portid, struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[100];
	struct rte_flow_action actions[10];
	struct rte_flow *flow;

	// Pattern struct
	struct rte_flow_item_ipv4 ipv4_spec;
	memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));

	static struct rte_flow_item_tcp tcp_spec;
	memset(&tcp_spec, 0, sizeof(struct rte_flow_item_tcp));

	// Action struct
  	uint16_t queues[4];
	struct rte_flow_action_rss rss;
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

    	rss.types = ETH_RSS_NONFRAG_IPV4_TCP;
    	rss.level = 0;
    	rss.func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;

    	rss.queue_num = 4;
		queues[0] = 0;
		queues[1] = 1;
		queues[2] = 2;
		queues[3] = 3;
    	rss.queue = queues;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
		actions[actionCount].conf = &rss;
		actionCount++;

		mark.id = 123;
		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_MARK;
		actions[actionCount].conf = &mark;
		actionCount++;

		actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
		actionCount++;

		flow = rte_flow_create(portid, &attr, pattern, actions, error);
		if (flow == NULL) {
			return -1;
		}

	}
	return 0;
}

```

## Copy Packet Offset to mbuf <a name="copyoffset"></a>
Normally `mbuf->data_off  = RTE_PKTMBUF_HEADROOM`, which is the offset to the beginnig of the packet data.
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

To enable *copy packet offset to mbuf*, set the following in common_base:

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

## Use NTPL Filters Addition (Making an Ethernet over MPLS Filter) <a name="ntplfilter"></a>
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
This is the string that will be embedded into the resulting NTPL filter expression. Ensure that the string will not break the NTPL filter expression. See below for an example.

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
The string "TunnelType==EoMPLS" is now embedded into to the NTPL filter expression, and the resulting filter is changed to an ethernet over MPLS filter matching the inner layer3 protocol.

#### tunnel
This tells the driver whether the following rte_flow filter items should be inner or outer filter items.

In the above example, tunnel=RTE_FLOW_NTPL_TUNNEL, which makes the filter an inner layer3 filter. If tunnel=RTE_FLOW_NTPL_NO_TUNNEL, the filter is now an outer layer3 filter.
```
KeyType[name=KT3;Access=partial;Bank=0;colorinfo=true;tag=port0]={64}
KeyDef[name=KDEF3;KeyType=KT3;tag=port0]=(Layer3Header[12]/64)
KeyList[KeySet=3;KeyType=KT3;color=305419896;tag=port0]=(0x0A0000019F140606)
assign[priority=1;Descriptor=DYN3,length=22,colorbits=32;streamid=0;tag=port0]=(Layer3Protocol==IPV4) and TunnelType==EoMPLS and port==0 and Key(KDEF3)==3
```
`InnerLayer3Header` and `InnerLayer3Protocol` is now changed to `Layer3Header` and `Layer3Protocol`.

Other NTPL filter expressions can be used as long as it does not break the resulting NTPL filter expression.

## Packet decoding offload <a name="hwdecode"></a>
Packet decoding can be made by the SmartNIC hardware by using the action `RTE_FLOW_ACTION_TYPE_FLAG`. This causes the hardware to fill out the packet type field in the mbuf structure and return the offset to layer3 and layer4 or innerlayer3 and innerlayer4.

The mbuf fields set:

| mbuf field | description |
|------------|-------------|
| mbuf->packet_type | The packet type according to rte_mbuf_ptype.h |
| mbuf->hash.fdir.lo | Either layer3 or innerlayer3 offset |
| mbuf->hash.fdir.hi | Either layer4 or innerlayer4 offset |
| mbuf->ol_flags | PKT_RX_FDIR_FLX and PKT_RX_FDIR<br> if packet_type, hash.fdir.lo and hash.fdir.hi contains valid values |

> Note: `RTE_FLOW_ACTION_TYPE_MARK` cannot be used as the same time as `RTE_FLOW_ACTION_TYPE_FLAG` and no valid hash value is returned.

> Note: The [Copy Packet Offset to mbuf](#copyoffset) feature must not be enabled. `CONFIG_RTE_LIBRTE_PMD_NTACC_COPY_OFFSET=n`

#### Layer3 and Layer4 packet decoding offload <a name="hwl3l4"></a> 
The following is an example on how layer3 and layer4 hardware decoding is used.

The filter setup:
```
memset(&attr, 0, sizeof(attr));
attr.ingress = 1;
attr.priority = 10;

for (protoTel = 0; protoTel < 6; protoTel++) {
  uint32_t actionCount = 0;
  uint32_t patternCount = 0;

  memset(&pattern, 0, sizeof(pattern));
  memset(&actions, 0, sizeof(actions));

  switch (protoTel)
  {
  case 0:
    // Creates a filter that will match all other packets, that the ones below
    break;
  case 1:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
    patternCount++;
    break;
  case 2:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV6;
    patternCount++;
    break;
  case 3:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_UDP;
    patternCount++;
    break;
  case 4:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TCP;
    patternCount++;
    break;
  case 5:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_SCTP;
    patternCount++;
    break;
  }

  pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
  patternCount++;

  if (numQueues > 1) {
    rss.func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
    rss.level = 0;
    rss.types  = ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_SCTP;
    rss.queue_num = numQueues;
    for (i = 0; i < numQueues; i++) {
      queues[i] = i;
    }
    rss.queue = queues;
    actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
    actions[actionCount].conf = &rss;
    actionCount++;
  }
  else {
    queue.index = 0;
    actions[actionCount].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[actionCount].conf = &queue;
    actionCount++;
  }

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_FLAG;
  actionCount++;

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
  actionCount++;

  if (rte_flow_create(port, &attr, pattern, actions, &error) == NULL) {
    printf("ERROR: %s\n", error.message);
    return -1;
  }
```

The handling of the packet decoding:
```
// Packet must be an IP packet
if (mb->packet_type <= 1) {
  // No valid packet type is found 
  return 0; 
}

switch (mb->packet_type & RTE_PTYPE_L3_MASK)
{
case RTE_PTYPE_L3_IPV4:
  {
    struct ipv4_hdr *pIPv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr *, mb->hash.fdir.lo);
    pIPv4_hdr->src_addr; // Layer3 IPv4 source address 
    pIPv4_hdr->dst_addr; // Layer3 IPv4 destination address
    break;
  }
case RTE_PTYPE_L3_IPV6:
  {
    struct ipv6_hdr *pIPv6_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv6_hdr *, mb->hash.fdir.lo);
    pIPv6_hdr->src_addr; // Layer3 IPv6 source address
    pIPv6_hdr->dst_addr; // Layer3 IPv6 destination address
    break;
  }
}

switch (mb->packet_type & RTE_PTYPE_L4_MASK)
{
case RTE_PTYPE_L4_UDP:
  {
    struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(mb, struct udp_hdr *, mb->hash.fdir.hi);
    udp_hdr->src_port; // Layer4 UDP source port
    udp_hdr->dst_port; // Layer4 UDP destination port
    break;
  }
case RTE_PTYPE_L4_TCP:
  {
    const struct tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(mb, struct tcp_hdr *, mb->hash.fdir.hi);
    tcp_hdr->src_port; // Layer4 TCP source port
    tcp_hdr->dst_port; // Layer4 TCP destination port
    break;
  }
case RTE_PTYPE_L4_SCTP:
  {
    const struct sctp_hdr *sctp_hdr = rte_pktmbuf_mtod_offset(mb, struct sctp_hdr *, mb->hash.fdir.hi);
    sctp_hdr->src_port; // Layer4 SCTP source port
    sctp_hdr->dst_port; // Layer4 SCTP destination port
    break;
  }
}
```

#### Inner most Layer3 and Layer4 packet decoding offload <a name="hwil3il4"></a> 
If the inner most layers are wanted, the following example can be used.

The filter setup:
```
// The filter for the outer layers
// Note that priority must be lower (higer number) than the filters
// for the inner layers
 
attr.ingress = 1;
attr.priority = 2;
for (int tel = 0; tel < 5; tel++) {
  memset(&pattern, 0, sizeof(pattern));
  uint32_t patternCount = 0;
  switch (tel)
  {
  case 0:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
    patternCount++;
    break;
  case 1:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV6;
    patternCount++;
    break;
  case 2:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TCP;
    patternCount++;
    break;
  case 3:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_UDP;
    patternCount++;
    break;
  case 4:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_SCTP;
    patternCount++;
    break;
  }

  pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
  patternCount++;

  memset(&actions, 0, sizeof(actions));
  uint32_t actionCount = 0;

  rss.types = ETH_RSS_NONFRAG_IPV4_TCP;
  rss.level = 0;
  rss.func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;

  rss.queue_num = 2;
  queues[0] = 0;
  queues[1] = 1;
  rss.queue = queues;
  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
  actions[actionCount].conf = &rss;
  actionCount++;

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_FLAG;
  actionCount++;

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
  actionCount++;

  flow = rte_flow_create(portid, &attr, pattern, actions, error);
  if (flow == NULL) {
    return -1;
  }
}

// The filter for the inner layers
// Note that priority must be higher (lower number) than the filters
// for the outer layers
 
attr.ingress = 1;
attr.priority = 1;
for (int tel = 0; tel < 5; tel++) {
  memset(&pattern, 0, sizeof(pattern));
  uint32_t patternCount = 0;

  // Activate inner layer filters. The type of tunnel is don't care
  pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TUNNEL;
  patternCount++;

  switch (tel)
  {
  case 0:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV4;
    patternCount++;
    break;
  case 1:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_IPV6;
    patternCount++;
    break;
  case 2:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_TCP;
    patternCount++;
    break;
  case 3:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_UDP;
    patternCount++;
    break;
  case 4:
    pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_SCTP;
    patternCount++;
    break;
  }

  pattern[patternCount].type = RTE_FLOW_ITEM_TYPE_END;
  patternCount++;

  memset(&actions, 0, sizeof(actions));
  uint32_t actionCount = 0;

  rss.types = ETH_RSS_NONFRAG_IPV4_TCP;
  rss.level = 0;
  rss.func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;

  rss.queue_num = 2;
  queues[0] = 0;
  queues[1] = 1;
  rss.queue = queues;
  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_RSS;
  actions[actionCount].conf = &rss;
  actionCount++;

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_FLAG;
  actionCount++;

  actions[actionCount].type = RTE_FLOW_ACTION_TYPE_END;
  actionCount++;

  flow = rte_flow_create(portid, &attr, pattern, actions, error);
  if (flow == NULL) {
    return -1;
  }
}
```
The handling of the packet decoding of the inner most layers:
```
if (mb->packet_type & RTE_PTYPE_INNER_L3_MASK) {
  switch (mb->packet_type & RTE_PTYPE_INNER_L3_MASK)
  {
  case RTE_PTYPE_INNER_L3_IPV4:
    {
      struct ipv4_hdr *pIPv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr *, mb->hash.fdir.lo);
      pIPv4_hdr-src_addr;
      pIPv4_hdr->dst_addr;
      break;
    }
  case RTE_PTYPE_INNER_L3_IPV6:
    {
      struct ipv6_hdr *pIPv6_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv6_hdr *, mb->hash.fdir.lo & 0xFFFF);
      pIPv6_hdr->src_addr;
      pIPv6_hdr->dst_addr;
      break;
    }
  }
}
else if (mb->packet_type & RTE_PTYPE_L3_MASK) {
  switch (mb->packet_type & RTE_PTYPE_L3_MASK)
  {
  case RTE_PTYPE_L3_IPV4:
    {
      struct ipv4_hdr *pIPv4_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv4_hdr *, mb->hash.fdir.lo);
      pIPv4_hdr->src_addr;
      pIPv4_hdr->dst_addr;
      break;
    }
  case RTE_PTYPE_L3_IPV6:
    {
      struct ipv6_hdr *pIPv6_hdr = rte_pktmbuf_mtod_offset(mb, struct ipv6_hdr *, mb->hash.fdir.lo & 0xFFFF);
      pIPv6_hdr->src_addr;
      pIPv6_hdr->dst_addr;
      break;
    }
  }
}

if (mb->packet_type & RTE_PTYPE_INNER_L4_MASK) {
  switch (mb->packet_type & RTE_PTYPE_INNER_L4_MASK)
  {
   case RTE_PTYPE_INNER_L4_SCTP:
    {
      const struct sctp_hdr *sctp_hdr = rte_pktmbuf_mtod_offset(mb, struct sctp_hdr *, mb->hash.fdir.hi);
      sctp_hdr->src_port;
      sctp_hdr->dst_port;
      break;
    }
  case RTE_PTYPE_INNER_L4_TCP:
    {
      const struct tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(mb, struct tcp_hdr *, mb->hash.fdir.hi);
      tcp_hdr->src_port;
      tcp_hdr->dst_port;
      break;
    }
  case RTE_PTYPE_INNER_L4_UDP:
    {
      struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(mb, struct udp_hdr *, mb->hash.fdir.hi);
      udp_hdr->src_port;
      udp_hdr->dst_port;
      break;
    }
  }
}
else if (mb->packet_type & RTE_PTYPE_L4_MASK) {
  switch (mb->packet_type & RTE_PTYPE_L4_MASK)
  {
   case RTE_PTYPE_L4_SCTP:
    {
      const struct sctp_hdr *sctp_hdr = rte_pktmbuf_mtod_offset(mb, struct sctp_hdr *, mb->hash.fdir.hi);
      sctp_hdr->src_port;
      sctp_hdr->dst_port;
      break;
    }
  case RTE_PTYPE_L4_TCP:
    {
      const struct tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(mb, struct tcp_hdr *, mb->hash.fdir.hi);
      tcp_hdr->src_port;
      tcp_hdr->dst_port;
      break;
    }
  case RTE_PTYPE_L4_UDP:
    {
      struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(mb, struct udp_hdr *, mb->hash.fdir.hi);
      udp_hdr->src_port;
      udp_hdr->dst_port;
      break;
    }
  }
}

```
#### More packet decoding types  <a name="hwmoredecode"></a>
It is possible to add more packet decoding types if wanted. The following table shows which packet types that is made available in mbuf->packet_types according to which item filter made.

| Item filter type          | mbuf->packet_type                                        |
|---------------------------|----------------------------------------------------------|
| RTE_FLOW_ITEM_TYPE_ETH    | RTE_PTYPE_INNER_L2_ETHER<br>RTE_PTYPE_L2_ETHER           |
| RTE_FLOW_ITEM_TYPE_IPV4   | RTE_PTYPE_INNER_L3_IPV4<br>RTE_PTYPE_L3_IPV4             |
| RTE_FLOW_ITEM_TYPE_IPV6   | RTE_PTYPE_INNER_L3_IPV6<br>RTE_PTYPE_L3_IPV6             |
| RTE_FLOW_ITEM_TYPE_SCTP   | RTE_PTYPE_INNER_L4_SCTP<br>RTE_PTYPE_L4_SCTP             |
| RTE_FLOW_ITEM_TYPE_TCP    | RTE_PTYPE_INNER_L4_TCP<br>RTE_PTYPE_L4_TCP               |
| RTE_FLOW_ITEM_TYPE_UDP    | RTE_PTYPE_INNER_L4_UDP<br>RTE_PTYPE_L4_UDP               |
| RTE_FLOW_ITEM_TYPE_ICMP   | RTE_PTYPE_INNER_L4_ICMP<br>RTE_PTYPE_L4_ICMP             |
| RTE_FLOW_ITEM_TYPE_VLAN   | RTE_PTYPE_INNER_L2_ETHER_VLAN<br>RTE_PTYPE_L2_ETHER_VLAN | 
| RTE_FLOW_ITEM_TYPE_GRE    | RTE_PTYPE_TUNNEL_GRE                                     |
| RTE_FLOW_ITEM_TYPE_GTPU   | RTE_PTYPE_TUNNEL_GTPU                                    | 
| RTE_FLOW_ITEM_TYPE_GTPC   | RTE_PTYPE_TUNNEL_GTPC                                    |
| RTE_FLOW_ITEM_TYPE_VXLAN  | RTE_PTYPE_TUNNEL_VXLAN                                   |
| RTE_FLOW_ITEM_TYPE_NVGRE  | RTE_PTYPE_TUNNEL_NVGRE                                   |  
| RTE_FLOW_ITEM_TYPE_IPinIP | RTE_PTYPE_TUNNEL_IP                                      |







## Use External Buffers (Zero copy)<a name="externalbuffer"></a>
When a packet is received it is normally copied from the Napatech buffer to a mbuf buffer. When external buffers are used a pointer to the Napatech internal buffer is returned in the mbuf and thereby no packet copying is done. This was possible from DPDK version 18.05.

Due to DPDK buffer housekeeping this will only be faster for packets larger than 250 bytes. For packets smaller than 250 bytes it will be the same speed or a little bit slower depending on the packet size.

#### Limitation using external buffers<a name="limitexternalbuffer"></a>
There are some limitations when using external buffers.

1. Packets must be released in order
   It is not allowed to keep a packet for further processing. It must be released in order and before the next burst of packets are fetched. If a packet has to be kept for further processing it must be copied to a new mbuf.
2. There is no headroom
   Headroom is located in the mbuf buffer and as this buffer is replaced by a pointer to the Napatech internal buffer, threre is no free space for headroom.

#### Enable external buffers<a name="enableexternalbuffer"></a>
To enable *Using external buffers*, run following command when using meson:

`meson setup --reconfigure -Dntacc_external_buffers=true`

#### Using/detecting external buffers<a name="detectexternalbuffer"></a>
When receiving a mbuf it is possible to detect whether or not it contains an external buffer. 
If the flag `EXT_ATTACHED_MBUF` is set in `mbuf->ol_flags` then the mbuf contains an external buffer.
The macro `RTE_MBUF_HAS_EXTBUF` can be used.

``` 
if (RTE_MBUF_HAS_EXTBUF(mbuf))
  // External buffer in mbuf
else
  // Normal mbuf
``` 

> Note: **Use External hosbuffers is replacing the Napatech proprietary Contiguous memory batching** 


