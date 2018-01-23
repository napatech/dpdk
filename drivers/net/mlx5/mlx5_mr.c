/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
 *   Copyright 2016 Mellanox.
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

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mempool.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"

struct mr_update_mempool_data {
	struct rte_eth_dev *dev;
	struct mlx5_mr_cache *lkp_tbl;
	uint16_t tbl_sz;
};

/**
 * Look up LKEY from given lookup table by Binary Search, store the last index
 * and return searched LKEY.
 *
 * @param lkp_tbl
 *   Pointer to lookup table.
 * @param n
 *   Size of lookup table.
 * @param[out] idx
 *   Pointer to index. Even on searh failure, returns index where it stops
 *   searching so that index can be used when inserting a new entry.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKEY on success, UINT32_MAX on no match.
 */
static uint32_t
mlx5_mr_lookup(struct mlx5_mr_cache *lkp_tbl, uint16_t n, uint16_t *idx,
	       uintptr_t addr)
{
	uint16_t base = 0;

	/* First entry must be NULL for comparison. */
	assert(n == 0 || (lkp_tbl[0].start == 0 &&
			  lkp_tbl[0].lkey == UINT32_MAX));
	/* Binary search. */
	do {
		register uint16_t delta = n >> 1;

		if (addr < lkp_tbl[base + delta].start) {
			n = delta;
		} else {
			base += delta;
			n -= delta;
		}
	} while (n > 1);
	assert(addr >= lkp_tbl[base].start);
	*idx = base;
	if (addr < lkp_tbl[base].end)
		return lkp_tbl[base].lkey;
	/* Not found. */
	return UINT32_MAX;
}

/**
 * Insert an entry to LKEY lookup table.
 *
 * @param lkp_tbl
 *   Pointer to lookup table. The size of array must be enough to add one more
 *   entry.
 * @param n
 *   Size of lookup table.
 * @param entry
 *   Pointer to new entry to insert.
 *
 * @return
 *   Size of returning lookup table.
 */
static int
mlx5_mr_insert(struct mlx5_mr_cache *lkp_tbl, uint16_t n,
	       struct mlx5_mr_cache *entry)
{
	uint16_t idx = 0;
	size_t shift;

	/* Check if entry exist. */
	if (mlx5_mr_lookup(lkp_tbl, n, &idx, entry->start) != UINT32_MAX)
		return n;
	/* Insert entry. */
	++idx;
	shift = (n - idx) * sizeof(struct mlx5_mr_cache);
	if (shift)
		memmove(&lkp_tbl[idx + 1], &lkp_tbl[idx], shift);
	lkp_tbl[idx] = *entry;
	DRV_LOG(DEBUG, "%p: inserted lkp_tbl[%u], start = 0x%lx, end = 0x%lx",
		(void *)lkp_tbl, idx, lkp_tbl[idx].start, lkp_tbl[idx].end);
	return n + 1;
}

/**
 * Incrementally update LKEY lookup table for a specific address from registered
 * Memory Regions.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param lkp_tbl
 *   Pointer to lookup table to fill. The size of array must be at least
 *   (priv->mr_n + 1).
 * @param n
 *   Size of lookup table.
 * @param addr
 *   Search key.
 *
 * @return
 *   Size of returning lookup table.
 */
static int
mlx5_mr_update_addr(struct rte_eth_dev *dev, struct mlx5_mr_cache *lkp_tbl,
		    uint16_t n, uintptr_t addr)
{
	struct priv *priv = dev->data->dev_private;
	uint16_t idx;
	uint32_t ret __rte_unused;

	if (n == 0) {
		/* First entry must be NULL for comparison. */
		lkp_tbl[n++] = (struct mlx5_mr_cache) {
			.lkey = UINT32_MAX,
		};
	}
	ret = mlx5_mr_lookup(*priv->mr_cache, MR_TABLE_SZ(priv->mr_n),
			     &idx, addr);
	/* Lookup must succeed, the global cache is all-inclusive. */
	assert(ret != UINT32_MAX);
	DRV_LOG(DEBUG, "port %u adding LKEY (0x%x) for addr 0x%lx",
		dev->data->port_id, (*priv->mr_cache)[idx].lkey, addr);
	return mlx5_mr_insert(lkp_tbl, n, &(*priv->mr_cache)[idx]);
}

/**
 * Bottom-half of LKEY search on datapath. Firstly search in cache_bh[] and if
 * misses, search in the global MR cache table and update the new entry to
 * per-queue local caches.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   LKEY on success.
 */
static inline uint32_t
mlx5_mr_mb2mr_bh(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		 uintptr_t addr)
{
	uint32_t lkey;
	uint16_t bh_idx = 0;
	struct mlx5_mr_cache *mr_cache = &mr_ctrl->cache[mr_ctrl->head];

	/* Binary-search MR translation table. */
	lkey = mlx5_mr_lookup(*mr_ctrl->cache_bh, mr_ctrl->bh_n, &bh_idx, addr);
	if (likely(lkey != UINT32_MAX)) {
		/* Update cache. */
		*mr_cache = (*mr_ctrl->cache_bh)[bh_idx];
		mr_ctrl->mru = mr_ctrl->head;
		/* Point to the next victim, the oldest. */
		mr_ctrl->head = (mr_ctrl->head + 1) % MLX5_MR_CACHE_N;
		return lkey;
	}
	/* Missed in the per-queue lookup table. Search in the global cache. */
	mr_ctrl->bh_n = mlx5_mr_update_addr(dev, *mr_ctrl->cache_bh,
					    mr_ctrl->bh_n, addr);
	/* Search again with updated entries. */
	lkey = mlx5_mr_lookup(*mr_ctrl->cache_bh, mr_ctrl->bh_n, &bh_idx, addr);
	/* Must always succeed. */
	assert(lkey != UINT32_MAX);
	/* Update cache. */
	*mr_cache = (*mr_ctrl->cache_bh)[bh_idx];
	mr_ctrl->mru = mr_ctrl->head;
	/* Point to the next victim, the oldest. */
	mr_ctrl->head = (mr_ctrl->head + 1) % MLX5_MR_CACHE_N;
	return lkey;
}

/**
 * Bottom-half of mlx5_rx_mb2mr() if search on mr_cache_bh[] fails.
 *
 * @param rxq
 *   Pointer to Rx queue structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   LKEY on success.
 */
uint32_t
mlx5_rx_mb2mr_bh(struct mlx5_rxq_data *rxq, uintptr_t addr)
{
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);

	DRV_LOG(DEBUG,
		"port %u not found in rxq->mr_cache[], last-hit=%u, head=%u",
		PORT_ID(rxq_ctrl->priv), rxq->mr_ctrl.mru, rxq->mr_ctrl.head);
	return mlx5_mr_mb2mr_bh(ETH_DEV(rxq_ctrl->priv), &rxq->mr_ctrl, addr);
}

/**
 * Bottom-half of mlx5_tx_mb2mr() if search on cache_bh[] fails.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   LKEY on success.
 */
uint32_t
mlx5_tx_mb2mr_bh(struct mlx5_txq_data *txq, uintptr_t addr)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);

	DRV_LOG(DEBUG,
		"port %u not found in txq->mr_cache[], last-hit=%u, head=%u",
		PORT_ID(txq_ctrl->priv), txq->mr_ctrl.mru, txq->mr_ctrl.head);
	return mlx5_mr_mb2mr_bh(ETH_DEV(txq_ctrl->priv), &txq->mr_ctrl, addr);
}

/* Called by mr_update_mempool() when iterating the memory chunks. */
static void
mr_update_mempool_cb(struct rte_mempool *mp __rte_unused,
		    void *opaque, struct rte_mempool_memhdr *memhdr,
		    unsigned int mem_idx __rte_unused)
{
	struct mr_update_mempool_data *data = opaque;

	DRV_LOG(DEBUG, "port %u adding chunk[%u] of %s",
		data->dev->data->port_id, mem_idx, mp->name);
	data->tbl_sz =
		mlx5_mr_update_addr(data->dev, data->lkp_tbl, data->tbl_sz,
				    (uintptr_t)memhdr->addr);
}

/**
 * Incrementally update LKEY lookup table for a specific Memory Pool from
 * registered Memory Regions.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] lkp_tbl
 *   Pointer to lookup table to fill. The size of array must be at least
 *   (priv->static_mr_n + 1).
 * @param n
 *   Size of lookup table.
 * @param[in] mp
 *   Pointer to Memory Pool.
 *
 * @return
 *   Size of returning lookup table.
 */
int
mlx5_mr_update_mp(struct rte_eth_dev *dev, struct mlx5_mr_cache *lkp_tbl,
		  uint16_t n, struct rte_mempool *mp)
{
	struct mr_update_mempool_data data = {
		.dev = dev,
		.lkp_tbl = lkp_tbl,
		.tbl_sz = n
	};

	rte_mempool_mem_iter(mp, mr_update_mempool_cb, &data);
	return data.tbl_sz;
}

/* Called by qsort() to compare MR entries. */
static int
mr_comp_addr(const void *m1, const void *m2)
{
	const struct mlx5_mr *mi1 = m1;
	const struct mlx5_mr *mi2 = m2;

	if (mi1->memseg->addr < mi2->memseg->addr)
		return -1;
	else if (mi1->memseg->addr > mi2->memseg->addr)
		return 1;
	else
		return 0;
}

/**
 * Register entire physical memory to Verbs.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mr_register_memseg(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	struct mlx5_mr *mr;
	struct mlx5_mr_cache *mr_cache;
	unsigned int i;

	if (priv->mr_n != 0)
		return 0;
	/* Count the existing memsegs in the system. */
	for (i = 0; (i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL); ++i)
		++priv->mr_n;
	priv->mr = rte_calloc(__func__, priv->mr_n, sizeof(*mr), 0);
	if (priv->mr == NULL) {
		DRV_LOG(ERR,
			"port %u cannot allocate memory for array of static MR",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->mr_cache = rte_calloc(__func__, MR_TABLE_SZ(priv->mr_n),
				    sizeof(*mr_cache), 0);
	if (priv->mr_cache == NULL) {
		DRV_LOG(ERR,
			"port %u cannot allocate memory for array of MR cache",
			dev->data->port_id);
		rte_free(priv->mr);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	for (i = 0; i < priv->mr_n; ++i) {
		mr = &(*priv->mr)[i];
		mr->memseg = &ms[i];
		mr->ibv_mr = ibv_reg_mr(priv->pd,
					mr->memseg->addr, mr->memseg->len,
					IBV_ACCESS_LOCAL_WRITE);
		if (mr->ibv_mr == NULL) {
			rte_dump_physmem_layout(stderr);
			DRV_LOG(ERR, "port %u cannot register memseg[%u]",
				dev->data->port_id, i);
			goto error;
		}
	}
	/* Sort by virtual address. */
	qsort(*priv->mr, priv->mr_n, sizeof(struct mlx5_mr), mr_comp_addr);
	/* First entry must be NULL for comparison. */
	(*priv->mr_cache)[0] = (struct mlx5_mr_cache) {
		.lkey = UINT32_MAX,
	};
	/* Compile global all-inclusive MR cache table. */
	for (i = 0; i < priv->mr_n; ++i) {
		mr = &(*priv->mr)[i];
		mr_cache = &(*priv->mr_cache)[i + 1];
		/* Paranoid, mr[] must be sorted. */
		assert(i == 0 || mr->memseg->addr > (mr - 1)->memseg->addr);
		*mr_cache = (struct mlx5_mr_cache) {
			.start = (uintptr_t)mr->memseg->addr,
			.end = (uintptr_t)mr->memseg->addr + mr->memseg->len,
			.lkey = rte_cpu_to_be_32(mr->ibv_mr->lkey)
		};
	}
	return 0;
error:
	for (i = 0; i < priv->mr_n; ++i) {
		mr = &(*priv->mr)[i];
		if (mr->ibv_mr != NULL)
			ibv_dereg_mr(mr->ibv_mr);
	}
	rte_free(priv->mr);
	rte_free(priv->mr_cache);
	rte_errno = ENOMEM;
	return -rte_errno;
}

/**
 * Deregister all Memory Regions.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_mr_deregister_memseg(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;

	if (priv->mr_n == 0)
		return;
	for (i = 0; i < priv->mr_n; ++i) {
		struct mlx5_mr *mr;

		mr = &(*priv->mr)[i];
		/* Physical memory can't be changed dynamically. */
		assert(mr->memseg != NULL);
		assert(mr->ibv_mr != NULL);
		ibv_dereg_mr(mr->ibv_mr);
	}
	rte_free(priv->mr);
	rte_free(priv->mr_cache);
	priv->mr = NULL;
	priv->mr_cache = NULL;
	priv->mr_n = 0;
}
