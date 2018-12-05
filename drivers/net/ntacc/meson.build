# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if host_machine.system() != 'linux'
        build = false
endif
sources = files('rte_eth_ntacc.c',
				'filter_ntacc.c')

includes += include_directories('/opt/napatech3/include')

allow_experimental_apis = true
