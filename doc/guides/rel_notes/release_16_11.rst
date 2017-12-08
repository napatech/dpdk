DPDK Release 16.11
==================

.. **Read this first.**

   The text below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text: ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      firefox build/doc/html/guides/rel_notes/release_16_11.html


New Features
------------

.. This section should contain new features added in this release. Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to understand
     the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like this.

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     This section is a comment. Make sure to start the actual text at the margin.


* **Added software parser for packet type.**

  * Added a new function ``rte_pktmbuf_read()`` to read the packet data from an
    mbuf chain, linearizing if required.
  * Added a new function ``rte_net_get_ptype()`` to parse an Ethernet packet
    in an mbuf chain and retrieve its packet type from software.
  * Added new functions ``rte_get_ptype_*()`` to dump a packet type as a string.

* **Improved offloads support in mbuf.**

  * Added a new function ``rte_raw_cksum_mbuf()`` to process the checksum of
    data embedded in an mbuf chain.
  * Added new Rx checksum flags in mbufs to describe more states: unknown,
    good, bad, or not present (useful for virtual drivers). This modification
    was done for IP and L4.
  * Added a new Rx LRO mbuf flag, used when packets are coalesced. This
    flag indicates that the segment size of original packets is known.

* **Added vhost-user dequeue zero copy support.**

  The copy in the dequeue path is avoided in order to improve the performance.
  In the VM2VM case, the boost is quite impressive. The bigger the packet size,
  the bigger performance boost you may get. However, for the VM2NIC case, there
  are some limitations, so the boost is not as  impressive as the VM2VM case.
  It may even drop quite a bit for small packets.

  For that reason, this feature is disabled by default. It can be enabled when
  the ``RTE_VHOST_USER_DEQUEUE_ZERO_COPY`` flag is set. Check the VHost section
  of the Programming Guide for more information.

* **Added vhost-user indirect descriptors support.**

  If the indirect descriptor feature is enabled, each packet sent by the guest
  will take exactly one slot in the enqueue virtqueue. Without this feature, as in
  the current version, even 64 bytes packets take two slots with Virtio PMD on guest
  side.

  The main impact is better performance for 0% packet loss use-cases, as it
  behaves as if the virtqueue size was enlarged, so more packets can be buffered
  in the case of system perturbations. On the downside, small performance degradations
  were measured when running micro-benchmarks.

* **Added vhost PMD xstats.**

  Added extended statistics to vhost PMD from a per port perspective.

* **Supported offloads with virtio.**

  Added support for the following offloads in virtio:

  * Rx/Tx checksums.
  * LRO.
  * TSO.

* **Added virtio NEON support for ARM.**

  Added NEON support for ARM based virtio.

* **Updated the ixgbe base driver.**

  Updated the ixgbe base driver, including the following changes:

  * Added X550em_a 10G PHY support.
  * Added support for flow control auto negotiation for X550em_a 1G PHY.
  * Added X550em_a FW ALEF support.
  * Increased mailbox version to ``ixgbe_mbox_api_13``.
  * Added two MAC operations for Hyper-V support.

* **Added APIs for VF management to the ixgbe PMD.**

  Eight new APIs have been added to the ixgbe PMD for VF management from the PF.
  The declarations for the API's can be found in ``rte_pmd_ixgbe.h``.

* **Updated the enic driver.**

  * Added update to use interrupt for link status checking instead of polling.
  * Added more flow director modes on UCS Blade with firmware version >= 2.0(13e).
  * Added full support for MTU update.
  * Added support for the ``rte_eth_rx_queue_count`` function.

* **Updated the mlx5 driver.**

  * Added support for RSS hash results.
  * Added several performance improvements.
  * Added several bug fixes.

* **Updated the QAT PMD.**

  The QAT PMD was updated with additional support for:

  * MD5_HMAC algorithm.
  * SHA224-HMAC algorithm.
  * SHA384-HMAC algorithm.
  * GMAC algorithm.
  * KASUMI (F8 and F9) algorithm.
  * 3DES algorithm.
  * NULL algorithm.
  * C3XXX device.
  * C62XX device.

* **Added openssl PMD.**

  A new crypto PMD has been added, which provides several ciphering and hashing algorithms.
  All cryptography operations use the Openssl library crypto API.

* **Updated the IPsec example.**

  Updated the IPsec example with the following support:

  * Configuration file support.
  * AES CBC IV generation with cipher forward function.
  * AES GCM/CTR mode.

* **Added support for new gcc -march option.**

  The GCC 4.9 ``-march`` option supports the Intel processor code names.
  The config option ``RTE_MACHINE`` can be used to pass code names to the compiler via the ``-march`` flag.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past tense.
     The title should contain the code/lib section like a commit message.
     Add the entries in alphabetic order in the relevant sections below.

   This section is a comment. Make sure to start the actual text at the margin.


Drivers
~~~~~~~

* **enic: Fixed several flow director issues.**

* **enic: Fixed inadvertent setting of L4 checksum ptype on ICMP packets.**

* **enic: Fixed high driver overhead when servicing Rx queues beyond the first.**



Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. Make sure to start the actual text at the margin.

* **L3fwd-power app does not work properly when Rx vector is enabled.**

  The L3fwd-power app doesn't work properly with some drivers in vector mode
  since the queue monitoring works differently between scalar and vector modes
  leading to incorrect frequency scaling. In addition, L3fwd-power application
  requires the mbuf to have correct packet type set but in some drivers the
  vector mode must be disabled for this.

  Therefore, in order to use L3fwd-power, vector mode should be disabled
  via the config file.

* **Digest address must be supplied for crypto auth operation on QAT PMD.**

  The cryptodev API specifies that if the rte_crypto_sym_op.digest.data field,
  and by inference the digest.phys_addr field which points to the same location,
  is not set for an auth operation the driver is to understand that the digest
  result is located immediately following the region over which the digest is
  computed. The QAT PMD doesn't correctly handle this case and reads and writes
  to an incorrect location.

  Callers can workaround this by always supplying the digest virtual and
  physical address fields in the rte_crypto_sym_op for an auth operation.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Make sure to start the actual text at the margin.

* The driver naming convention has been changed to make them more
  consistent. It especially impacts ``--vdev`` arguments. For example
  ``eth_pcap`` becomes ``net_pcap`` and ``cryptodev_aesni_mb_pmd`` becomes
  ``crypto_aesni_mb``.

  For backward compatibility an alias feature has been enabled to support the
  original names.

* The log history has been removed.

* The ``rte_ivshmem`` feature (including library and EAL code) has been removed
  in 16.11 because it had some design issues which were not planned to be fixed.

* The ``file_name`` data type of ``struct rte_port_source_params`` and
  ``struct rte_port_sink_params`` is changed from ``char *`` to ``const char *``.

* **Improved device/driver hierarchy and generalized hotplugging.**

  The device and driver relationship has been restructured by introducing generic
  classes. This paves the way for having PCI, VDEV and other device types as
  instantiated objects rather than classes in themselves. Hotplugging has also
  been generalized into EAL so that Ethernet or crypto devices can use the
  common infrastructure.

  * Removed ``pmd_type`` as a way of segregation of devices.
  * Moved ``numa_node`` and ``devargs`` into ``rte_driver`` from
    ``rte_pci_driver``. These can now be used by any instantiated object of
    ``rte_driver``.
  * Added ``rte_device`` class and all PCI and VDEV devices inherit from it
  * Renamed devinit/devuninit handlers to probe/remove to make it more
    semantically correct with respect to the device <=> driver relationship.
  * Moved hotplugging support to EAL. Hereafter, PCI and vdev can use the
    APIs ``rte_eal_dev_attach`` and ``rte_eal_dev_detach``.
  * Renamed helpers and support macros to make them more synonymous
    with their device types
    (e.g. ``PMD_REGISTER_DRIVER`` => ``RTE_PMD_REGISTER_PCI``).
  * Device naming functions have been generalized from ethdev and cryptodev
    to EAL. ``rte_eal_pci_device_name`` has been introduced for obtaining
    unique device name from PCI Domain-BDF description.
  * Virtual device registration APIs have been added: ``rte_eal_vdrv_register``
    and ``rte_eal_vdrv_unregister``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced in
     the previous releases and made in this release. Use fixed width quotes for
     ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Make sure to start the actual text at the margin.



Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     libethdev.so.4
     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2



The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_cryptodev.so.2
     librte_distributor.so.1
   + librte_eal.so.3
   + librte_ethdev.so.5
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
     librte_mempool.so.2
     librte_meter.so.1
     librte_net.so.1
     librte_pdump.so.1
     librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested with this release.

   The format is:

   #. Platform name.

      * Platform details.
      * Platform details.

   This section is a comment. Make sure to start the actual text at the margin.

#. SuperMicro 1U

   - BIOS: 1.0c
   - Processor: Intel(R) Atom(TM) CPU C2758 @ 2.40GHz

#. SuperMicro 1U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
   - Onboard NIC: Intel(R) X552/X557-AT (2x10G)

     - Firmware-version: 0x800001cf
     - Device ID (PF/VF): 8086:15ad /8086:15a8

   - kernel driver version: 4.2.5 (ixgbe)

#. SuperMicro 2U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz

#. Intel(R) Server board S2600GZ

   - BIOS: SE5C600.86B.02.02.0002.122320131210
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board W2600CR

   - BIOS: SE5C600.86B.02.01.0002.082220131453
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board S2600CWT

   - BIOS: SE5C610.86B.01.01.0009.060120151350
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.01.01.0005.101720141054
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.11.01.0044.090120151156
   - Processor: Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz

#. Intel(R) Server board S2600WTT

   - Processor: Intel(R) Xeon(R) CPU E5-2697 v2 @ 2.70GHz

#. Intel(R) Server

   - Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz

#. IBM(R) Power8(R)

   - Machine type-model: 8247-22L
   - Firmware FW810.21 (SV810_108)
   - Processor: POWER8E (raw), AltiVec supported


Tested NICs
-----------

.. This section should contain a list of NICs that were tested with this release.

   The format is:

   #. NIC name.

      * NIC details.
      * NIC details.

   This section is a comment. Make sure to start the actual text at the margin.

#. Intel(R) Ethernet Controller X540-AT2

   - Firmware version: 0x80000389
   - Device id (pf): 8086:1528
   - Driver version: 3.23.2 (ixgbe)

#. Intel(R) 82599ES 10 Gigabit Ethernet Controller

   - Firmware version: 0x61bf0001
   - Device id (pf/vf): 8086:10fb / 8086:10ed
   - Driver version: 4.0.1-k (ixgbe)

#. Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

   - Firmware version: 0x800001cf
   - Device id (pf/vf): 8086:15ad / 8086:15a8
   - Driver version: 4.2.5 (ixgbe)

#. Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA1 (1x40G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1584 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1583 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Corporation I350 Gigabit Network Connection

   - Firmware version: 1.48, 0x800006e7
   - Device id (pf/vf): 8086:1521 / 8086:1520
   - Driver version: 5.2.13-k (igb)

#. Intel(R) Ethernet Multi-host Controller FM10000

   - Firmware version: N/A
   - Device id (pf/vf): 8086:15d0
   - Driver version: 0.17.0.9 (fm10k)

#. Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1015
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 14.17.1010

#. Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1015
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 14.17.1010


Tested OSes
-----------

.. This section should contain a list of OSes that were tested with this release.
   The format is as follows, in alphabetical order:

   * CentOS 7.0
   * Fedora 23
   * Fedora 24
   * FreeBSD 10.3
   * Red Hat Enterprise Linux 7.2
   * SUSE Enterprise Linux 12
   * Ubuntu 15.10
   * Ubuntu 16.04 LTS
   * Wind River Linux 8

   This section is a comment. Make sure to start the actual text at the margin.

* CentOS 7.2
* Fedora 23
* Fedora 24
* FreeBSD 10.3
* FreeBSD 11
* Red Hat Enterprise Linux Server release 6.7 (Santiago)
* Red Hat Enterprise Linux Server release 7.0 (Maipo)
* Red Hat Enterprise Linux Server release 7.2 (Maipo)
* SUSE Enterprise Linux 12
* Wind River Linux 6.0.0.26
* Wind River Linux 8
* Ubuntu 14.04
* Ubuntu 15.04
* Ubuntu 16.04

Fixes in 16.11 LTS Release
--------------------------

16.11.1
~~~~~~~

* app/test: fix symmetric session free in crypto perf tests
* app/testpmd: fix check for invalid ports
* app/testpmd: fix static build link ordering
* crypto/aesni_gcm: fix IV size in capabilities
* crypto/aesni_gcm: fix J0 padding bytes
* crypto/aesni_mb: fix incorrect crypto session
* crypto/openssl: fix extra bytes written at end of data
* crypto/openssl: fix indentation in guide
* crypto/qat: fix IV size in capabilities
* crypto/qat: fix to avoid buffer overwrite in OOP case
* cryptodev: fix crash on null dereference
* cryptodev: fix loop in device query
* devargs: reset driver name pointer on parsing failure
* drivers/crypto: fix different auth/cipher keys
* ethdev: check maximum number of queues for statistics
* ethdev: fix extended statistics name index
* ethdev: fix port data mismatched in multiple process model
* ethdev: fix port lookup if none
* ethdev: remove invalid function from version map
* examples/ethtool: fix driver information
* examples/ethtool: fix querying non-PCI devices
* examples/ip_pipeline: fix coremask limitation
* examples/ip_pipeline: fix parsing of pass-through pipeline
* examples/l2fwd-crypto: fix overflow
* examples/vhost: fix calculation of mbuf count
* examples/vhost: fix lcore initialization
* mempool: fix API documentation
* mempool: fix stack handler dequeue
* net/af_packet: fix fd use after free
* net/bnx2x: fix Rx mode configuration
* net/cxgbe/base: initialize variable before reading EEPROM
* net/cxgbe: fix parenthesis on bitwise operation
* net/ena: fix setting host attributes
* net/enic: fix hardcoding of some flow director masks
* net/enic: fix memory leak with oversized Tx packets
* net/enic: remove unnecessary function parameter attributes
* net/i40e: enable auto link update for 25G
* net/i40e: fix Rx checksum flag
* net/i40e: fix TC bandwidth definition
* net/i40e: fix VF reset flow
* net/i40e: fix checksum flag in x86 vector Rx
* net/i40e: fix crash in close
* net/i40e: fix deletion of all macvlan filters
* net/i40e: fix ethertype filter on X722
* net/i40e: fix link update delay
* net/i40e: fix logging for Tx free threshold check
* net/i40e: fix segment number in reassemble process
* net/i40e: fix wrong return value when handling PF message
* net/i40e: fix xstats value mapping
* net/i40evf: fix casting between structs
* net/i40evf: fix reporting of imissed packets
* net/ixgbe: fix blocked interrupts
* net/ixgbe: fix received packets number for ARM
* net/ixgbe: fix received packets number for ARM NEON
* net/ixgbevf: fix max packet length
* net/mlx5: fix RSS hash result for flows
* net/mlx5: fix Rx packet validation and type
* net/mlx5: fix Tx doorbell
* net/mlx5: fix endianness in Tx completion queue
* net/mlx5: fix inconsistent link status
* net/mlx5: fix leak when starvation occurs
* net/mlx5: fix link status query
* net/mlx5: fix memory leak when parsing device params
* net/mlx5: fix missing inline attributes
* net/mlx5: fix updating total length of multi-packet send
* net/mlx: fix IPv4 and IPv6 packet type
* net/nfp: fix VLAN offload flags check
* net/nfp: fix typo in Tx offload capabilities
* net/pcap: fix timestamps in output pcap file
* net/qede/base: fix FreeBSD build
* net/qede: add vendor/device id info
* net/qede: fix PF fastpath status block index
* net/qede: fix filtering code
* net/qede: fix function declaration
* net/qede: fix per queue statisitics
* net/qede: fix resource leak
* net/vhost: fix socket file deleted on stop
* net/vhost: fix unix socket not removed as closing
* net/virtio-user: fix not properly reset device
* net/virtio-user: fix wrongly get/set features
* net/virtio: fix build without virtio-user
* net/virtio: fix crash when number of virtio devices > 1
* net/virtio: fix multiple process support
* net/virtio: fix performance regression due to TSO
* net/virtio: fix rewriting LSC flag
* net/virtio: fix wrong Rx/Tx method for secondary process
* net/virtio: optimize header reset on any layout
* net/virtio: store IO port info locally
* net/virtio: store PCI operators pointer locally
* net/vmxnet3: fix Rx deadlock
* pci: fix check of mknod
* pmdinfogen: fix endianness with cross-compilation
* pmdinfogen: fix null dereference
* sched: fix crash when freeing port
* usertools: fix active interface detection when binding
* vdev: fix detaching with alias
* vfio: fix file descriptor leak in multi-process
* vhost: allow many vhost-user ports
* vhost: do not GSO when no header is present
* vhost: fix dead loop in enqueue path
* vhost: fix guest/host physical address mapping
* vhost: fix long stall of negotiation
* vhost: fix memory leak

16.11.2
~~~~~~~

* app/testpmd: fix TC mapping in DCB init config
* app/testpmd: fix crash at mbuf pool creation
* app/testpmd: fix exit without freeing resources
* app/testpmd: fix init config for multi-queue mode
* app/testpmd: fix number of mbufs in pool
* app: enable HW CRC strip by default
* crypto/openssl: fix AAD capabilities for AES-GCM
* crypto/openssl: fix AES-GCM capability
* crypto/qat: fix AES-GCM authentication length
* crypto/qat: fix IV zero physical address
* crypto/qat: fix dequeue statistics
* cryptodev: fix API digest length comments
* doc: add limitation of AAD size to QAT guide
* doc: explain zlib dependency for bnx2x
* eal/linux: fix build with glibc 2.25
* eal: fix debug macro redefinition
* examples/ip_fragmentation: fix check of packet type
* examples/l2fwd-crypto: fix AEAD tests when AAD is zero
* examples/l2fwd-crypto: fix packets array index
* examples/l2fwd-crypto: fix padding calculation
* examples/l3fwd-power: fix Rx descriptor size
* examples/l3fwd-power: fix handling no Rx queue
* examples/load_balancer: fix Tx flush
* examples/multi_process: fix timer update
* examples/performance-thread: fix build on FreeBSD
* examples/performance-thread: fix build on FreeBSD 10.0
* examples/performance-thread: fix compilation on Suse 11 SP2
* examples/quota_watermark: fix requirement for 2M pages
* examples: enable HW CRC strip by default
* examples: fix build clean on FreeBSD
* kni: fix build with kernel 4.11
* kni: fix crash caused by freeing mempool
* kni: fix possible memory leak
* mbuf: fix missing includes in exported header
* mk: fix lib filtering when linking app
* mk: fix quoting for ARM mtune argument
* mk: fix shell errors when building with clang
* net/bnx2x: fix transmit queue free threshold
* net/bonding: allow configuring jumbo frames without slaves
* net/bonding: fix updating slave link status
* net/cxgbe: fix possible null pointer dereference
* net/e1000/base: fix multicast setting in VF
* net/ena: cleanup if refilling of Rx descriptors fails
* net/ena: fix Rx descriptors allocation
* net/ena: fix delayed cleanup of Rx descriptors
* net/ena: fix return of hash control flushing
* net/fm10k: fix memory overflow in 32-bit SSE Rx
* net/fm10k: fix pointer cast
* net/i40e/base: fix potential out of bound array access
* net/i40e: add missing 25G link speed
* net/i40e: ensure vector mode is not used with QinQ
* net/i40e: fix TC bitmap of VEB
* net/i40e: fix VF link speed
* net/i40e: fix VF link status update
* net/i40e: fix allocation check
* net/i40e: fix compile error
* net/i40e: fix hash input set on X722
* net/i40e: fix incorrect packet index reference
* net/i40e: fix mbuf alloc failed counter
* net/i40e: fix memory overflow in 32-bit SSE Rx
* net/i40e: fix setup when bulk is disabled
* net/igb: fix VF MAC address setting
* net/igb: fix VF MAC address setting
* net/ixgbe/base: fix build error
* net/ixgbe: fix Rx queue blocking issue
* net/ixgbe: fix TC bandwidth setting
* net/ixgbe: fix VF Rx mode for allmulticast disabled
* net/ixgbe: fix all queues drop setting of DCB
* net/ixgbe: fix memory overflow in 32-bit SSE Rx
* net/ixgbe: fix multi-queue mode check in SRIOV mode
* net/ixgbe: fix setting MTU on stopped device
* net/ixgbevf: set xstats id values
* net/mlx4: fix Rx after mbuf alloc failure
* net/mlx4: fix returned values upon failed probing
* net/mlx4: update link status upon probing with LSC
* net/mlx5: fix Tx when first segment size is too short
* net/mlx5: fix VLAN stripping indication
* net/mlx5: fix an uninitialized variable
* net/mlx5: fix returned values upon failed probing
* net/mlx5: fix reusing Rx/Tx queues
* net/mlx5: fix supported packets types
* net/nfp: fix packet/data length conversion
* net/pcap: fix using mbuf after freeing it
* net/qede/base: fix find zero bit macro
* net/qede: fix FW version string for VF
* net/qede: fix default MAC address handling
* net/qede: fix fastpath rings reset phase
* net/qede: fix missing UDP protocol in RSS offload types
* net/thunderx: fix 32-bit build
* net/thunderx: fix build on FreeBSD
* net/thunderx: fix deadlock in Rx path
* net/thunderx: fix stats access out of bounds
* net/virtio-user: fix address on 32-bit system
* net/virtio-user: fix overflow
* net/virtio: disable LSC interrupt if MSIX not enabled
* net/virtio: fix MSI-X for modern devices
* net/virtio: fix crash when closing twice
* net/virtio: fix link status always being up
* net/virtio: fix link status always down
* net/virtio: fix queue notify
* net/vmxnet3: fix build with gcc 7
* net/vmxnet3: fix queue size changes
* net: fix stripped VLAN flag for offload emulation
* nic_uio: fix device binding at boot
* pci: fix device registration on FreeBSD
* test/cmdline: fix missing break in switch
* test/mempool: free mempool on exit
* test: enable HW CRC strip by default
* vfio: fix disabling INTx
* vfio: fix secondary process start
* vhost: change log levels in client mode
* vhost: fix dequeue zero copy
* vhost: fix false sharing
* vhost: fix fd leaks for vhost-user server mode
* vhost: fix max queues
* vhost: fix multiple queue not enabled for old kernels
* vhost: fix use after free

16.11.3
~~~~~~~

* contigmem: do not zero pages during each mmap
* contigmem: free allocated memory on error
* crypto/aesni_mb: fix HMAC supported key sizes
* cryptodev: fix device stop function
* crypto/openssl: fix HMAC supported key sizes
* crypto/qat: fix HMAC supported key sizes
* crypto/qat: fix NULL authentication hang
* crypto/qat: fix SHA384-HMAC block size
* doc: remove incorrect limitation on AESNI-MB PMD
* doc: remove incorrect limitation on QAT PMD
* eal: fix config file path when checking process
* examples/l2fwd-crypto: fix application help
* examples/l2fwd-crypto: fix option parsing
* examples/l2fwd-crypto: fix padding
* examples/l3fwd: fix IPv6 packet type parse
* examples/qos_sched: fix build for less lcores
* ip_frag: free mbufs on reassembly table destroy
* kni: fix build with gcc 7.1
* lpm: fix index of tbl8
* mbuf: fix debug checks for headroom and tailroom
* mbuf: fix doxygen comment of bulk alloc
* mbuf: fix VXLAN port in comment
* mem: fix malloc element resize with padding
* net/bnxt: check invalid L2 filter id
* net/bnxt: enable default VNIC allocation
* net/bnxt: fix autoneg on 10GBase-T links
* net/bnxt: fix get link config
* net/bnxt: fix reporting of link status
* net/bnxt: fix set link config
* net/bnxt: fix set link config
* net/bnxt: fix vnic cleanup
* net/bnxt: free filter before reusing it
* net/bonding: change link status check to no-wait
* net/bonding: fix number of bonding Tx/Rx queues
* net/bonding: fix when NTT flag updated
* net/cxgbe: fix port statistics
* net/e1000: fix LSC interrupt
* net/ena: fix cleanup of the Tx bufs
* net/enic: fix build with gcc 7.1
* net/enic: fix crash when freeing 0 packet to mempool
* net/fm10k: initialize link status in device start
* net/i40e: add return value checks
* net/i40e/base: fix Tx error stats on VF
* net/i40e: exclude internal packet's byte count
* net/i40e: fix division by 0
* net/i40e: fix ethertype filter for new FW
* net/i40e: fix link down and negotiation
* net/i40e: fix Rx data segment buffer length
* net/i40e: fix VF statistics
* net/igb: fix add/delete of flex filters
* net/igb: fix checksum valid flags
* net/igb: fix flex filter length
* net/ixgbe: fix mirror rule index overflow
* net/ixgbe: fix Rx/Tx queue interrupt for x550 devices
* net/mlx4: fix mbuf poisoning in debug code
* net/mlx4: fix probe failure report
* net/mlx5: fix build with gcc 7.1
* net/mlx5: fix completion buffer size
* net/mlx5: fix exception handling
* net/mlx5: fix inconsistent link status query
* net/mlx5: fix redundant free of Tx buffer
* net/qede: fix chip details print
* net/virtio: do not claim to support LRO
* net/virtio: do not falsely claim to do IP checksum
* net/virtio-user: fix crash when detaching device
* net/virtio: zero the whole memory zone
* net/vmxnet3: fix filtering on promiscuous disabling
* net/vmxnet3: fix receive queue memory leak
* Revert "ip_frag: free mbufs on reassembly table destroy"
* test/bonding: fix memory corruptions
* test/bonding: fix mode 4 names
* test/bonding: fix namespace of the RSS tests
* test/bonding: fix parameters of a balance Tx
* test/crypto: fix overflow
* test/crypto: fix wrong AAD setting
* vhost: fix checking of device features
* vhost: fix guest pages memory leak
* vhost: fix IP checksum
* vhost: fix TCP checksum
* vhost: make page logging atomic

16.11.4
~~~~~~~

* app/testpmd: fix forwarding between non consecutive ports
* app/testpmd: fix invalid port id parameters
* app/testpmd: fix mapping of user priority to DCB TC
* app/testpmd: fix packet throughput after stats reset
* app/testpmd: fix RSS structure initialisation
* app/testpmd: fix topology error message
* buildtools: check allocation error in pmdinfogen
* buildtools: fix icc build
* cmdline: fix compilation with -Og
* cmdline: fix warning for unused return value
* config: fix bnx2x option for armv7a
* cryptodev: fix build with -Ofast
* crypto/qat: fix SHA512-HMAC supported key size
* drivers/crypto: use snprintf return value correctly
* eal/bsd: fix missing interrupt stub functions
* eal: copy raw strings taken from command line
* eal: fix auxv open check for ARM and PPC
* eal/x86: fix atomic cmpset
* examples/ipsec-secgw: fix IPv6 payload length
* examples/ipsec-secgw: fix IP version check
* examples/l2fwd-cat: fix build with PQOS 1.4
* examples/l2fwd-crypto: fix uninitialized errno value
* examples/l2fwd_fork: fix message pool init
* examples/l3fwd-acl: check fseek return
* examples/multi_process: fix received message length
* examples/performance-thread: check thread creation
* examples/performance-thread: fix out-of-bounds sched array
* examples/performance-thread: fix out-of-bounds tls array
* examples/qos_sched: fix uninitialized config
* hash: fix eviction counter
* kni: fix build on RHEL 7.4
* kni: fix build on SLE12 SP3
* kni: fix ethtool build with kernel 4.11
* lpm6: fix compilation with -Og
* mem: fix malloc element free in debug mode
* net/bnxt: fix a bit shift operation
* net/bnxt: fix an issue with broadcast traffic
* net/bnxt: fix a potential null pointer dereference
* net/bnxt: fix interrupt handler
* net/bnxt: fix link handling and configuration
* net/bnxt: fix Rx offload capability
* net/bnxt: fix Tx offload capability
* net/bnxt: set checksum offload flags correctly
* net/bnxt: update status of Rx IP/L4 CKSUM
* net/bonding: fix LACP slave deactivate behavioral
* net/cxgbe: fix memory leak
* net/enic: fix assignment
* net/enic: fix packet loss after MTU change
* net/enic: fix possible null pointer dereference
* net: fix inner L2 length in packet type parser
* net/i40e/base: fix bool definition
* net/i40e: fix clear xstats bug in VF
* net/i40e: fix flexible payload configuration
* net/i40e: fix flow control watermark mismatch
* net/i40e: fix i40evf MAC filter table
* net/i40e: fix mbuf free in vector Tx
* net/i40e: fix memory leak if VF init fails
* net/i40e: fix mirror rule reset when port is closed
* net/i40e: fix mirror with firmware 6.0
* net/i40e: fix packet count for PF
* net/i40e: fix PF notify issue when VF is not up
* net/i40e: fix Rx packets number for NEON
* net/i40e: fix Rx queue interrupt mapping in VF
* net/i40e: fix uninitialized variable
* net/i40e: fix variable assignment
* net/i40e: fix VF cannot forward packets issue
* net/i40e: fix VFIO interrupt mapping in VF
* net/igb: fix memcpy length
* net/igb: fix Rx interrupt with VFIO and MSI-X
* net/ixgbe: fix adding a mirror rule
* net/ixgbe: fix mapping of user priority to TC
* net/ixgbe: fix PF DCB info
* net/ixgbe: fix uninitialized variable
* net/ixgbe: fix VFIO interrupt mapping in VF
* net/ixgbe: fix VF RX hang
* net/mlx5: fix clang build
* net/mlx5: fix clang compilation error
* net/mlx5: fix link speed bitmasks
* net/mlx5: fix probe failure report
* net/mlx5: fix Tx stats error counter definition
* net/mlx5: fix Tx stats error counter logic
* net/mlx5: improve stack usage during link update
* net/nfp: fix RSS
* net/nfp: fix stats struct initial value
* net/pcap: fix memory leak in dumper open
* net/qede/base: fix API return types
* net/qede/base: fix division by zero
* net/qede/base: fix for VF malicious indication
* net/qede/base: fix macros to check chip revision/metal
* net/qede/base: fix number of app table entries
* net/qede/base: fix return code to align with FW
* net/qede/base: fix to use a passed ptt handle
* net/qede: fix icc build
* net/virtio: fix compilation with -Og
* net/virtio: fix mbuf port for simple Rx function
* net/virtio: fix queue setup consistency
* net/virtio: fix Tx packet length stats
* net/virtio: fix untrusted scalar value
* net/virtio: flush Rx queues on start
* net/vmxnet3: fix dereference before null check
* net/vmxnet3: fix MAC address set
* net/vmxnet3: fix memory leak when releasing queues
* pdump: fix possible mbuf leak on failure
* ring: guarantee load/load order in enqueue and dequeue
* test: fix assignment operation
* test/memzone: fix memory leak
* test/pmd_perf: fix crash with multiple devices
* timer: use 64-bit specific code on more platforms
* uio: fix compilation with -Og
* usertools: fix device binding with python 3
* vfio: fix close unchecked file descriptor
