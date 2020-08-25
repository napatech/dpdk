# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if host_machine.system() != 'linux'
        build = false
endif

#cflags += '-DCOPY_OFFSET0=InnerLayer3Header'

if get_option('ntacc_external_buffers') == true
  cflags += '-DUSE_EXTERNAL_BUFFER'
endif

if get_option('ntacc_use_sw_stat') == true
  cflags += '-DUSE_SW_STAT'
endif

if get_option('ntacc_no_default_filter') == true
  cflags += '-DDO_NOT_CREATE_DEFAULT_FILTER'
endif

sources = files('rte_eth_ntacc.c', 'filter_ntacc.c')

cmd = run_command('sh', '-c', 'echo $NAPATECH3_PATH')
PATH = cmd.stdout().strip()
INC_VAR = PATH + '/include'
LIB_VAR = '-DNAPATECH3_LIB_PATH="' + PATH + '/lib"'

cflags += LIB_VAR

includes += include_directories(INC_VAR)

allow_experimental_apis = true
