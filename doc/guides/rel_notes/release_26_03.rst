.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 26.03
==================

New Features
------------

* **Added custom memory allocation hooks in ACL library.**

  Added a hook API mechanism
  allowing applications to provide their own allocation and free functions
  for ACL runtime memory.

* **Updated AMD axgbe ethernet driver.**

  * Added support for V4000 Krackan2e.

* **Updated AF_PACKET ethernet driver.**

  * Added support for multi-segment mbuf reception to handle jumbo frames
    with standard mbuf sizes when scatter Rx offload is enabled.

* **Updated CESNET nfb ethernet driver.**

  * The timestamp value has been updated to make it usable.
  * The DPDK port has been changed to represent just one Ethernet port
    instead of all Ethernet ports on the NIC.
  * Added ``port`` device argument to select a subset of all ports.
  * Added firmware version, correct Ethernet link speed and maximum MTU reporting.
  * Common CESNET-NDK-based adapters have been added,
    including the FB2CGHH (Silicom Denmark) and XpressSX AGI-FH400G (Reflex CES).
  * Added support for configuration of the RS-FEC mode, link up / down state, and the Rx MTU.

* **Updated Google Virtual Ethernet (gve) driver.**

  * Added application-initiated device reset.
  * Added support for receive flow steering.

* **Updated Huawei hinic3 ethernet driver.**

  * Added support for Huawei's new SPx NICs, including SP230 and SP920 (DPU).
  * Added support for GENEVE tunnel TSO and IP-in-IP tunnel TSO on the SP230.
  * Added support for VXLAN-GPE checksum on the SP620.
  * Added support for tunnel packet outer UDP checksum.
  * Added support for QinQ on the SP620.

* **Updated Intel idpf ethernet driver.**

  * Added support for time sync features.

* **Updated Intel iavf driver.**

  * Added support for pre and post VF reset callbacks.

* **Updated Intel ice driver.**

  * Added flow API support for L2TPv2 over UDP.

* **Updated Intel idpf driver.**

  * Added AVX2 vectorized split queue Rx and Tx paths.

* **Updated Marvell cnxk net driver.**

  * Added out-of-place support for CN20K SoC.
  * Added plain packet reassembly support for CN20K SoC.
  * Added IPsec Rx inject support for CN20K SoC.

* **Updated ZTE zxdh ethernet driver.**

  * Added support for modifying queue depth.
  * Optimized queue allocation resources.
  * Added support for setting link speed and getting auto-negotiation status.
  * Added support for secondary processes.
  * Added support for GENEVE TSO and tunnel outer UDP Rx checksum.

* **Added 256-NEA/NCA/NIA algorithms in cryptodev library.**

  Added support for the following wireless algorithms:
  * NEA4, NIA4, NCA4: Snow 5G confidentiality, integrity and AEAD modes.
  * NEA5, NIA5, NCA5: AES 256 confidentiality, integrity and AEAD modes.
  * NEA6, NIA6, NCA6: ZUC 256 confidentiality, integrity and AEAD modes.

* **Updated Marvell cnxk crypto driver.**

  * Added support for Snow 5G NEA4/NIA4 and ZUC 256 NEA6/NIA6 for CN20K platform.

* **Updated openssl crypto driver.**

  * Added support for AES-XTS cipher algorithm.
  * Added support for SHAKE-128 and SHAKE-256 authentication algorithms.
  * Added support for SHA3-224, SHA3-256, SHA3-384, and SHA3-512 hash algorithms
    and their HMAC variants.

* **Added automatic deferred free on hash data overwrite.**

  When RCU is configured with a ``free_key_data_func`` callback,
  ``rte_hash_add_key_data`` now automatically defers
  freeing the old data pointer on key overwrite via the RCU defer queue.

* **Added Ctrl+L support to cmdline library.**

  Added handling of the key combination Ctrl+L
  to clear the screen before redisplaying the prompt.


Removed Items
-------------

* **Discontinued support for AMD Solarflare SFN7xxx family boards.**

  7000 series adaptors are out of support in terms of hardware.

* **Removed the SSE vector paths from some Intel drivers.**

  The SSE path was not widely used, so it was removed
  from the i40e, iavf and ice drivers.
  Each of these drivers has faster vector paths (AVX2 and AVX-512)
  which have feature parity with the SSE paths,
  and a fallback scalar path which also has feature parity.


API Changes
-----------

* **Added additional length checks for name parameter lengths.**

  Several library functions now have additional name length checks
  instead of silently truncating.

  * lpm: name must be less than ``RTE_LPM_NAMESIZE``.
  * hash: name parameter must be less than ``RTE_HASH_NAMESIZE``.
  * efd: name must be less than ``RTE_EFD_NAMESIZE``.
  * tailq: name must be less than ``RTE_TAILQ_NAMESIZE``.
  * cfgfile: name must be less than ``CFG_NAME_LEN``
    and value must be less than ``CFG_VALUE_LEN``.

* **Updated the pcapng library.**

  * The length of comment strings is now validated.
    Maximum allowable length is 2^16-1 because of the pcapng file format.


ABI Changes
-----------

* No ABI change that would break compatibility with 25.11.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Xeon\ |reg| 6740E  CPU @ 2.4GHz
    * Intel\ |reg| Xeon\ |reg| 6756P-B  CPU @ 2.2GHz
    * Intel\ |reg| Xeon\ |reg| 6760P  CPU @ 2.2GHz
    * Intel\ |reg| Xeon\ |reg| 6767P
    * Intel\ |reg| Xeon\ |reg| 6767P  CPU @ 2.4GHz
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480  CPU @ 1.9GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz  CPU @ 2.0GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz  CPU @ 2.0GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6430L
    * Intel\ |reg| Xeon\ |reg| Gold 6430L  CPU @ 1.9GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz  CPU @ 2.3GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H  CPU @ 2.1GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H  CPU @ 1.9GHz

  * OS:

    * Fedora 43
    * FreeBSD 15.0
    * Microsoft Azure Linux 3.0
    * OpenAnolis OS 8.10
    * openEuler 24.03 (LTS-SP2)
    * Red Hat Enterprise Linux Server release 10
    * Red Hat Enterprise Linux Server release 9.6
    * Ubuntu 24.04.3 LTS
    * Ubuntu 24.04.4 LTS
    * Vmware Exsi 9.0

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.91 0x8002147b 1.3909.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-of-tree): 2.4.5 (ice)
      * Driver version(in-tree): 6.8.0-87-generic (Ubuntu24.04.3 LTS) (ice)
      * OS Default DDP: 1.3.53.0
      * COMMS DDP: 1.3.61.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.91 0x800214ae 1.3909.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-of-tree): 2.4.5 (ice)
      * Driver version(in-tree): 6.6.119.3-1.azl3 (Microsoft Azure Linux 3.0) /
        6.12.0-55.9.1.el10_0.x86_64+rt (RHEL10) (ice)
      * OS Default DDP: 1.3.53.0
      * COMMS DDP: 1.3.61.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E830-CC for QSFP

      * Firmware version: 1.20 0x80017ef4 1.3909.0
      * Device id (pf/vf): 8086:12d2 / 8086:1889
      * Driver version: 2.4.5 (ice)
      * OS Default DDP: 1.3.53.0
      * COMMS DDP: 1.3.61.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 4.04 0x80007e5c 1.3885.0
      * Device id (pf/vf): 8086:579d / 8086:1889
      * Driver version: 2.4.5 (ice)
      * OS Default DDP: 1.3.53.0
      * COMMS DDP: 1.3.61.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.41 0x8000eda0 1.3909.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-of-tree): 6.3.4 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller X550

      * Firmware version: 0x80001860, 1.3105.0
      * Device id (pf/vf): 8086:1563 / 8086:1565
      * Driver version(out-of-tree): 6.3.4 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.56 0x800100c4 1.3909.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(in-tree): 6.12.0-55.9.1.el10_0.x86_64 (RHEL10) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.56 0x80010121 1.3909.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-of-tree): 2.28.16 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.56 0x80010101 1.3909.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-of-tree): 2.28.16 (i40e)
      * Driver version(in-tree): 6.8.0-101-generic (Ubuntu24.04.3 LTS) (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP (2x10G)

      * Firmware version: 6.51 0x80004430 1.3909.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-of-tree): 2.28.16 (i40e)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 6.8.0-88-generic (Ubuntu24.04.3 LTS) (igc)

    * Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100

      * Firmware version: ci-ts.release.2.0.0.11126
      * Device id (idpf/cpfl): 8086:1452/8086:1453
      * Driver version: 0.0.772 (idpf)

* Intel\ |reg| platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 9.2 (Plow)
    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Ubuntu 24.04
    * Ubuntu 22.04
    * SUSE Enterprise Linux 15 SP4

  * DOCA:

    * DOCA 3.3.0-088000 and above.

  * upstream kernel:

    * Linux 6.18.0 and above

  * rdma-core:

    * rdma-core-60.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.48.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.48.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.48.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-8 SuperNIC 400G  MT4131 - 900-9X81Q-00CN-STA (2x400G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1023
      * Firmware version: 40.48.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-9 SuperNIC 800G  MT4133 - 900-9X91E-00EB-STA_Ax (1x800G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1025
      * Firmware version: 82.47.0366 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.48.1000 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.48.1000 and above

  * Embedded software:

    * Ubuntu 24.04
    * MLNX_OFED 26.01-1.0.0.1
    * bf-bundle-3.3.0-202_26.01_ubuntu-24.04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 24.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.48.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.48.1000 and above

  * DOCA:

    * DOCA 3.3.0-088000 and above

* IBM Power 11 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * Power11 2.0 (pvr 0082 0200)

  * OS:

    * Red Hat Enterprise Linux 10.1 (6.12.0-124.45.1)
    * SUSE Linux Enterprise Server 15 SP7 (6.4.0-150700.53.31)

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-7 25GbE MCX713104AS-ADAT (4x25GbE)

      * Host interface: PCIe 4.0 x16
      * Driver version: 26.01-1.0.0
      * Firmware version: 28.47.1088

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200GbE MCX755106AS-HEAT (2x200GbE)

      * Host interface: PCIe 5.0x16 with x16 PCIe extension option
      * Driver version: 26.01-1.0.0
      * Firmware version: 28.47.1088

  * DOCA:

    * DOCA 3.3.0-088000
