HW accelerated FlowTable using rte_flow
=======================================

<code>examples/flowtable</code> contain a flow table example program using
rte_flow for HW offloading lookups.

When a flow is learned its index in the flow array is programmed via
rte_flow into HW. Packets containing an index is used directly to locate the
flow data instead of calculating a hash value and perform a hash lookup.

[Please read the Napatech blog for details and performance results](https://www.napatech.com/hw-acceleration-via-rte_flow/)



