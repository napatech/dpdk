/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_common.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   struct priv for the requested device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;

	if (priv_ifreq(priv, SIOCGIFHWADDR, &request))
		return -1;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
	return 0;
}

/**
 * Remove a MAC address from the internal array.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
static void
mlx5_internal_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	if (mlx5_is_secondary())
		return;
	assert(index < MLX5_MAX_MAC_ADDRESSES);
	if (is_zero_ether_addr(&dev->data->mac_addrs[index]))
		return;
	memset(&dev->data->mac_addrs[index], 0, sizeof(struct ether_addr));
}

/**
 * Adds a MAC address to the internal array.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success.
 */
static int
mlx5_internal_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
			   uint32_t index)
{
	unsigned int i;

	if (mlx5_is_secondary())
		return 0;
	if (index >= MLX5_MAX_MAC_ADDRESSES) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (is_zero_ether_addr(mac)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != MLX5_MAX_MAC_ADDRESSES); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == index)
			continue;
		if (memcmp(&dev->data->mac_addrs[i], mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error. */
		return EADDRINUSE;
	}
	dev->data->mac_addrs[index] = *mac;
	return 0;
}

/**
 * DPDK callback to remove a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	int ret;

	if (index >= MLX5_MAX_UC_MAC_ADDRESSES)
		return;
	mlx5_internal_mac_addr_remove(dev, index);
	if (!dev->data->promiscuous) {
		ret = mlx5_traffic_restart(dev);
		if (ret)
			ERROR("port %u cannot restart traffic: %s",
			      dev->data->port_id, strerror(rte_errno));
	}
}

/**
 * DPDK callback to add a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 * @param vmdq
 *   VMDq pool index to associate address with (ignored).
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac,
		  uint32_t index, uint32_t vmdq __rte_unused)
{
	int ret;

	if (index >= MLX5_MAX_UC_MAC_ADDRESSES) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_internal_mac_addr_add(dev, mac, index);
	if (ret < 0)
		return ret;
	if (!dev->data->promiscuous)
		return mlx5_traffic_restart(dev);
	return 0;
}

/**
 * DPDK callback to set primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 */
void
mlx5_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	if (mlx5_is_secondary())
		return;
	DEBUG("%p: setting primary MAC address", (void *)dev);
	mlx5_mac_addr_add(dev, mac_addr, 0, 0);
}

/**
 * DPDK callback to set multicast addresses list.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr_set
 *   Multicast MAC address pointer array.
 * @param nb_mac_addr
 *   Number of entries in the array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_set_mc_addr_list(struct rte_eth_dev *dev,
		      struct ether_addr *mc_addr_set, uint32_t nb_mc_addr)
{
	uint32_t i;
	int ret;

	for (i = MLX5_MAX_UC_MAC_ADDRESSES; i != MLX5_MAX_MAC_ADDRESSES; ++i)
		mlx5_internal_mac_addr_remove(dev, i);
	i = MLX5_MAX_UC_MAC_ADDRESSES;
	while (nb_mc_addr--) {
		ret = mlx5_internal_mac_addr_add(dev, mc_addr_set++, i++);
		if (ret)
			return ret;
	}
	if (!dev->data->promiscuous)
		return mlx5_traffic_restart(dev);
	return 0;
}
