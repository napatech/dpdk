/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_H_
#define _BNXT_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>
#include <rte_time.h>

#include "bnxt_cpr.h"

/* Vendor ID */
#define PCI_VENDOR_ID_BROADCOM		0x14E4

/* Device IDs */
#define BROADCOM_DEV_ID_STRATUS_NIC_VF1	0x1606
#define BROADCOM_DEV_ID_STRATUS_NIC_VF2	0x1609
#define BROADCOM_DEV_ID_STRATUS_NIC	0x1614
#define BROADCOM_DEV_ID_57414_VF	0x16c1
#define BROADCOM_DEV_ID_57301		0x16c8
#define BROADCOM_DEV_ID_57302		0x16c9
#define BROADCOM_DEV_ID_57304_PF	0x16ca
#define BROADCOM_DEV_ID_57304_VF	0x16cb
#define BROADCOM_DEV_ID_57417_MF	0x16cc
#define BROADCOM_DEV_ID_NS2		0x16cd
#define BROADCOM_DEV_ID_57311		0x16ce
#define BROADCOM_DEV_ID_57312		0x16cf
#define BROADCOM_DEV_ID_57402		0x16d0
#define BROADCOM_DEV_ID_57404		0x16d1
#define BROADCOM_DEV_ID_57406_PF	0x16d2
#define BROADCOM_DEV_ID_57406_VF	0x16d3
#define BROADCOM_DEV_ID_57402_MF	0x16d4
#define BROADCOM_DEV_ID_57407_RJ45	0x16d5
#define BROADCOM_DEV_ID_57412		0x16d6
#define BROADCOM_DEV_ID_57414		0x16d7
#define BROADCOM_DEV_ID_57416_RJ45	0x16d8
#define BROADCOM_DEV_ID_57417_RJ45	0x16d9
#define BROADCOM_DEV_ID_5741X_VF	0x16dc
#define BROADCOM_DEV_ID_57412_MF	0x16de
#define BROADCOM_DEV_ID_57314		0x16df
#define BROADCOM_DEV_ID_57317_RJ45	0x16e0
#define BROADCOM_DEV_ID_5731X_VF	0x16e1
#define BROADCOM_DEV_ID_57417_SFP	0x16e2
#define BROADCOM_DEV_ID_57416_SFP	0x16e3
#define BROADCOM_DEV_ID_57317_SFP	0x16e4
#define BROADCOM_DEV_ID_57404_MF	0x16e7
#define BROADCOM_DEV_ID_57406_MF	0x16e8
#define BROADCOM_DEV_ID_57407_SFP	0x16e9
#define BROADCOM_DEV_ID_57407_MF	0x16ea
#define BROADCOM_DEV_ID_57414_MF	0x16ec
#define BROADCOM_DEV_ID_57416_MF	0x16ee
#define BROADCOM_DEV_ID_58802		0xd802
#define BROADCOM_DEV_ID_58804		0xd804
#define BROADCOM_DEV_ID_58808		0x16f0
#define BROADCOM_DEV_ID_58802_VF	0xd800

#define BNXT_MAX_MTU		9574
#define VLAN_TAG_SIZE		4
#define BNXT_NUM_VLANS		2
#define BNXT_MAX_PKT_LEN	(BNXT_MAX_MTU + ETHER_HDR_LEN +	\
				 ETHER_CRC_LEN + \
				 (BNXT_NUM_VLANS * VLAN_TAG_SIZE))
/* FW adds extra 4 bytes for FCS */
#define BNXT_VNIC_MRU(mtu)\
	((mtu) + ETHER_HDR_LEN + VLAN_TAG_SIZE * BNXT_NUM_VLANS)
#define BNXT_VF_RSV_NUM_RSS_CTX	1
#define BNXT_VF_RSV_NUM_L2_CTX	4
/* TODO: For now, do not support VMDq/RFS on VFs. */
#define BNXT_VF_RSV_NUM_VNIC	1
#define BNXT_MAX_LED		4
#define BNXT_MIN_RING_DESC	16
#define BNXT_MAX_TX_RING_DESC	4096
#define BNXT_MAX_RX_RING_DESC	8192
#define BNXT_DB_SIZE		0x80

/* Chimp Communication Channel */
#define GRCPF_REG_CHIMP_CHANNEL_OFFSET		0x0
#define GRCPF_REG_CHIMP_COMM_TRIGGER		0x100
/* Kong Communication Channel */
#define GRCPF_REG_KONG_CHANNEL_OFFSET		0xA00
#define GRCPF_REG_KONG_COMM_TRIGGER		0xB00

#define BNXT_INT_LAT_TMR_MIN			75
#define BNXT_INT_LAT_TMR_MAX			150
#define BNXT_NUM_CMPL_AGGR_INT			36
#define BNXT_CMPL_AGGR_DMA_TMR			37
#define BNXT_NUM_CMPL_DMA_AGGR			36
#define BNXT_CMPL_AGGR_DMA_TMR_DURING_INT	50
#define BNXT_NUM_CMPL_DMA_AGGR_DURING_INT	12

struct bnxt_led_info {
	uint8_t      led_id;
	uint8_t      led_type;
	uint8_t      led_group_id;
	uint8_t      unused;
	uint16_t  led_state_caps;
#define BNXT_LED_ALT_BLINK_CAP(x)       ((x) &  \
	rte_cpu_to_le_16(HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_BLINKALT))

	uint16_t  led_color_caps;
};

struct bnxt_led_cfg {
	uint8_t led_id;
	uint8_t led_state;
	uint8_t led_color;
	uint8_t unused;
	uint16_t led_blink_on;
	uint16_t led_blink_off;
	uint8_t led_group_id;
	uint8_t rsvd;
};

#define BNXT_LED_DFLT_ENA                               \
	(HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_ID |             \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_STATE |          \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_ON |       \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_BLINK_OFF |      \
	 HWRM_PORT_LED_CFG_INPUT_ENABLES_LED0_GROUP_ID)

#define BNXT_LED_DFLT_ENA_SHIFT		6

#define BNXT_LED_DFLT_ENABLES(x)                        \
	rte_cpu_to_le_32(BNXT_LED_DFLT_ENA << (BNXT_LED_DFLT_ENA_SHIFT * (x)))

enum bnxt_hw_context {
	HW_CONTEXT_NONE     = 0,
	HW_CONTEXT_IS_RSS   = 1,
	HW_CONTEXT_IS_COS   = 2,
	HW_CONTEXT_IS_LB    = 3,
};

struct bnxt_vlan_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
} __attribute__((packed));

struct bnxt_vlan_antispoof_table_entry {
	uint16_t		tpid;
	uint16_t		vid;
	uint16_t		mask;
} __attribute__((packed));

struct bnxt_child_vf_info {
	void			*req_buf;
	struct bnxt_vlan_table_entry	*vlan_table;
	struct bnxt_vlan_antispoof_table_entry	*vlan_as_table;
	STAILQ_HEAD(, bnxt_filter_info)	filter;
	uint32_t		func_cfg_flags;
	uint32_t		l2_rx_mask;
	uint16_t		fid;
	uint16_t		max_tx_rate;
	uint16_t		dflt_vlan;
	uint16_t		vlan_count;
	uint8_t			mac_spoof_en;
	uint8_t			vlan_spoof_en;
	bool			random_mac;
	bool			persist_stats;
};

struct bnxt_pf_info {
#define BNXT_FIRST_PF_FID	1
#define BNXT_MAX_VFS(bp)	(bp->pf.max_vfs)
#define BNXT_TOTAL_VFS(bp)	((bp)->pf.total_vfs)
#define BNXT_FIRST_VF_FID	128
#define BNXT_PF_RINGS_USED(bp)	bnxt_get_num_queues(bp)
#define BNXT_PF_RINGS_AVAIL(bp)	(bp->pf.max_cp_rings - BNXT_PF_RINGS_USED(bp))
	uint16_t		port_id;
	uint16_t		first_vf_id;
	uint16_t		active_vfs;
	uint16_t		max_vfs;
	uint16_t		total_vfs; /* Total VFs possible.
					    * Not necessarily enabled.
					    */
	uint32_t		func_cfg_flags;
	void			*vf_req_buf;
	rte_iova_t		vf_req_buf_dma_addr;
	uint32_t		vf_req_fwd[8];
	uint16_t		total_vnics;
	struct bnxt_child_vf_info	*vf_info;
#define BNXT_EVB_MODE_NONE	0
#define BNXT_EVB_MODE_VEB	1
#define BNXT_EVB_MODE_VEPA	2
	uint8_t			evb_mode;
};

/* Max wait time is 10 * 100ms = 1s */
#define BNXT_LINK_WAIT_CNT	10
#define BNXT_LINK_WAIT_INTERVAL	100
struct bnxt_link_info {
	uint32_t		phy_flags;
	uint8_t			mac_type;
	uint8_t			phy_link_status;
	uint8_t			loop_back;
	uint8_t			link_up;
	uint8_t			duplex;
	uint8_t			pause;
	uint8_t			force_pause;
	uint8_t			auto_pause;
	uint8_t			auto_mode;
#define PHY_VER_LEN		3
	uint8_t			phy_ver[PHY_VER_LEN];
	uint16_t		link_speed;
	uint16_t		support_speeds;
	uint16_t		auto_link_speed;
	uint16_t		force_link_speed;
	uint16_t		auto_link_speed_mask;
	uint32_t		preemphasis;
	uint8_t			phy_type;
	uint8_t			media_type;
};

#define BNXT_COS_QUEUE_COUNT	8
struct bnxt_cos_queue_info {
	uint8_t	id;
	uint8_t	profile;
};

struct rte_flow {
	STAILQ_ENTRY(rte_flow) next;
	struct bnxt_filter_info *filter;
	struct bnxt_vnic_info	*vnic;
};

struct bnxt_ptp_cfg {
#define BNXT_GRCPF_REG_WINDOW_BASE_OUT  0x400
#define BNXT_GRCPF_REG_SYNC_TIME        0x480
#define BNXT_CYCLECOUNTER_MASK   0xffffffffffffffffULL
	struct rte_timecounter      tc;
	struct rte_timecounter      tx_tstamp_tc;
	struct rte_timecounter      rx_tstamp_tc;
	struct bnxt		*bp;
#define BNXT_MAX_TX_TS	1
	uint16_t			rxctl;
#define BNXT_PTP_MSG_SYNC			(1 << 0)
#define BNXT_PTP_MSG_DELAY_REQ			(1 << 1)
#define BNXT_PTP_MSG_PDELAY_REQ			(1 << 2)
#define BNXT_PTP_MSG_PDELAY_RESP		(1 << 3)
#define BNXT_PTP_MSG_FOLLOW_UP			(1 << 8)
#define BNXT_PTP_MSG_DELAY_RESP			(1 << 9)
#define BNXT_PTP_MSG_PDELAY_RESP_FOLLOW_UP	(1 << 10)
#define BNXT_PTP_MSG_ANNOUNCE			(1 << 11)
#define BNXT_PTP_MSG_SIGNALING			(1 << 12)
#define BNXT_PTP_MSG_MANAGEMENT			(1 << 13)
#define BNXT_PTP_MSG_EVENTS		(BNXT_PTP_MSG_SYNC |		\
					 BNXT_PTP_MSG_DELAY_REQ |	\
					 BNXT_PTP_MSG_PDELAY_REQ |	\
					 BNXT_PTP_MSG_PDELAY_RESP)
	uint8_t			tx_tstamp_en:1;
	int			rx_filter;

#define BNXT_PTP_RX_TS_L	0
#define BNXT_PTP_RX_TS_H	1
#define BNXT_PTP_RX_SEQ		2
#define BNXT_PTP_RX_FIFO	3
#define BNXT_PTP_RX_FIFO_PENDING 0x1
#define BNXT_PTP_RX_FIFO_ADV	4
#define BNXT_PTP_RX_REGS	5

#define BNXT_PTP_TX_TS_L	0
#define BNXT_PTP_TX_TS_H	1
#define BNXT_PTP_TX_SEQ		2
#define BNXT_PTP_TX_FIFO	3
#define BNXT_PTP_TX_FIFO_EMPTY	 0x2
#define BNXT_PTP_TX_REGS	4
	uint32_t			rx_regs[BNXT_PTP_RX_REGS];
	uint32_t			rx_mapped_regs[BNXT_PTP_RX_REGS];
	uint32_t			tx_regs[BNXT_PTP_TX_REGS];
	uint32_t			tx_mapped_regs[BNXT_PTP_TX_REGS];
};

struct bnxt_coal {
	uint16_t			num_cmpl_aggr_int;
	uint16_t			num_cmpl_dma_aggr;
	uint16_t			num_cmpl_dma_aggr_during_int;
	uint16_t			int_lat_tmr_max;
	uint16_t			int_lat_tmr_min;
	uint16_t			cmpl_aggr_dma_tmr;
	uint16_t			cmpl_aggr_dma_tmr_during_int;
};

#define BNXT_HWRM_SHORT_REQ_LEN		sizeof(struct hwrm_short_input)
struct bnxt {
	void				*bar0;

	struct rte_eth_dev		*eth_dev;
	struct rte_eth_rss_conf		rss_conf;
	struct rte_pci_device		*pdev;
	void				*doorbell_base;

	uint32_t		flags;
#define BNXT_FLAG_REGISTERED	(1 << 0)
#define BNXT_FLAG_VF		(1 << 1)
#define BNXT_FLAG_PORT_STATS	(1 << 2)
#define BNXT_FLAG_JUMBO		(1 << 3)
#define BNXT_FLAG_SHORT_CMD	(1 << 4)
#define BNXT_FLAG_UPDATE_HASH	(1 << 5)
#define BNXT_FLAG_PTP_SUPPORTED	(1 << 6)
#define BNXT_FLAG_MULTI_HOST    (1 << 7)
#define BNXT_FLAG_EXT_RX_PORT_STATS	(1 << 8)
#define BNXT_FLAG_EXT_TX_PORT_STATS	(1 << 9)
#define BNXT_FLAG_KONG_MB_EN	(1 << 10)
#define BNXT_FLAG_TRUSTED_VF_EN	(1 << 11)
#define BNXT_FLAG_DFLT_VNIC_SET	(1 << 12)
#define BNXT_FLAG_NEW_RM	(1 << 30)
#define BNXT_FLAG_INIT_DONE	(1U << 31)
#define BNXT_PF(bp)		(!((bp)->flags & BNXT_FLAG_VF))
#define BNXT_VF(bp)		((bp)->flags & BNXT_FLAG_VF)
#define BNXT_NPAR(bp)		((bp)->port_partition_type)
#define BNXT_MH(bp)             ((bp)->flags & BNXT_FLAG_MULTI_HOST)
#define BNXT_SINGLE_PF(bp)      (BNXT_PF(bp) && !BNXT_NPAR(bp) && !BNXT_MH(bp))
#define BNXT_USE_CHIMP_MB	0 //For non-CFA commands, everything uses Chimp.
#define BNXT_USE_KONG(bp)	((bp)->flags & BNXT_FLAG_KONG_MB_EN)
#define BNXT_VF_IS_TRUSTED(bp)	((bp)->flags & BNXT_FLAG_TRUSTED_VF_EN)

	unsigned int		rx_nr_rings;
	unsigned int		rx_cp_nr_rings;
	struct bnxt_rx_queue **rx_queues;
	const void		*rx_mem_zone;
	struct rx_port_stats    *hw_rx_port_stats;
	rte_iova_t		hw_rx_port_stats_map;
	struct rx_port_stats_ext    *hw_rx_port_stats_ext;
	rte_iova_t		hw_rx_port_stats_ext_map;
	uint16_t		fw_rx_port_stats_ext_size;

	unsigned int		tx_nr_rings;
	unsigned int		tx_cp_nr_rings;
	struct bnxt_tx_queue **tx_queues;
	const void		*tx_mem_zone;
	struct tx_port_stats    *hw_tx_port_stats;
	rte_iova_t		hw_tx_port_stats_map;
	struct tx_port_stats_ext    *hw_tx_port_stats_ext;
	rte_iova_t		hw_tx_port_stats_ext_map;
	uint16_t		fw_tx_port_stats_ext_size;

	/* Default completion ring */
	struct bnxt_cp_ring_info	*def_cp_ring;
	uint32_t		max_ring_grps;
	struct bnxt_ring_grp_info	*grp_info;

	unsigned int		nr_vnics;

#define BNXT_GET_DEFAULT_VNIC(bp)	(&(bp)->vnic_info[0])
	struct bnxt_vnic_info	*vnic_info;
	STAILQ_HEAD(, bnxt_vnic_info)	free_vnic_list;

	struct bnxt_filter_info	*filter_info;
	STAILQ_HEAD(, bnxt_filter_info)	free_filter_list;

	struct bnxt_irq         *irq_tbl;

#define MAX_NUM_MAC_ADDR	32
	uint8_t			mac_addr[ETHER_ADDR_LEN];

	uint16_t			hwrm_cmd_seq;
	uint16_t			kong_cmd_seq;
	void				*hwrm_cmd_resp_addr;
	rte_iova_t			hwrm_cmd_resp_dma_addr;
	void				*hwrm_short_cmd_req_addr;
	rte_iova_t			hwrm_short_cmd_req_dma_addr;
	rte_spinlock_t			hwrm_lock;
	uint16_t			max_req_len;
	uint16_t			max_resp_len;

	/* default command timeout value of 50ms */
#define HWRM_CMD_TIMEOUT	50000
	/* default HWRM request timeout value */
	uint32_t		hwrm_cmd_timeout;

	struct bnxt_link_info	link_info;
	struct bnxt_cos_queue_info	cos_queue[BNXT_COS_QUEUE_COUNT];
	uint8_t			tx_cosq_id;

	uint16_t		fw_fid;
	uint8_t			dflt_mac_addr[ETHER_ADDR_LEN];
	uint16_t		max_rsscos_ctx;
	uint16_t		max_cp_rings;
	uint16_t		max_tx_rings;
	uint16_t		max_rx_rings;
	uint16_t		max_l2_ctx;
	uint16_t		max_rx_em_flows;
	uint16_t		max_vnics;
	uint16_t		max_stat_ctx;
	uint16_t		vlan;
	struct bnxt_pf_info		pf;
	uint8_t			port_partition_type;
	uint8_t			dev_stopped;
	uint8_t			vxlan_port_cnt;
	uint8_t			geneve_port_cnt;
	uint16_t		vxlan_port;
	uint16_t		geneve_port;
	uint16_t		vxlan_fw_dst_port_id;
	uint16_t		geneve_fw_dst_port_id;
	uint32_t		fw_ver;
	uint32_t		hwrm_spec_code;

	struct bnxt_led_info	leds[BNXT_MAX_LED];
	uint8_t			num_leds;
	struct bnxt_ptp_cfg     *ptp_cfg;
	uint16_t		vf_resv_strategy;
};

int bnxt_mtu_set_op(struct rte_eth_dev *eth_dev, uint16_t new_mtu);
int bnxt_link_update_op(struct rte_eth_dev *eth_dev, int wait_to_complete);
int bnxt_rcv_msg_from_vf(struct bnxt *bp, uint16_t vf_id, void *msg);

bool is_bnxt_supported(struct rte_eth_dev *dev);
bool bnxt_stratus_device(struct bnxt *bp);
extern const struct rte_flow_ops bnxt_flow_ops;

extern int bnxt_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnxt_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt, ## args)
#endif
