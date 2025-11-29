SHELL:=/bin/bash
KERNEL=bandwidth
KERNEL_TOP=bandwidth
#KERNEL_ARGS=
PLATFORM=xilinx_u280_gen3x16_xdma_1_202211_1
#PLATFORM=xilinx_u200_gen3x16_xdma_2_202110_1

ARG_NUM_PARTS=1
ARG_NXCTRS=4

BUILD_DIR_PREFIX=/localhdd/arb26/workspace/builds/brahma/bandwidth/max/p${ARG_NUM_PARTS}_xctr${ARG_NXCTRS}

#XRT_INI_PATH=scripts/xrt.ini
MAX_SYNTH_JOBS=24
#STRATEGY="Explore"
#PLACEMENT_STRATEGY="EarlyBlockPlacement"

FLAGS_CO=-DTAPA_BUFFER_EXPLICIT_RELEASE -DSB_NUM_PARTITIONS=${ARG_NUM_PARTS} -DSB_NXCTRS=${ARG_NXCTRS}
#FLAGS_XO=--enable-buffer-exprel --enable-hbm-binding-adjustment --floorplan-pre-assignments manual_1.json --max-search-time 1200 --max-area-limit 0.95 --clock-period 4
FLAGS_XO_CFLAGS=--cflags="-DSB_NUM_PARTITIONS=${ARG_NUM_PARTS}" --cflags="-DSB_NXCTRS=${ARG_NXCTRS}"
FLAGS_XO=--enable-buffer-exprel --enable-hbm-binding-adjustment --max-search-time 1200 --max-area-limit 0.95 --clock-period 3.33 ${FLAGS_XO_CFLAGS}

include ../../utils/common.mk
