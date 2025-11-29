SHELL:=/bin/bash
WORKSPACE=/home/${USER}
KERNEL=add
KERNEL_TOP=VecAdd
#KERNEL_ARGS=
PLATFORM=xilinx_u280_gen3x16_xdma_1_202211_1
#PLATFORM=xilinx_u200_gen3x16_xdma_2_202110_1
BUILD_DIR_PREFIX=${WORKSPACE}/builds/apps/vadd-hb-p1

#XRT_INI_PATH=scripts/xrt.ini
MAX_SYNTH_JOBS=24
#STRATEGY="Explore"
#PLACEMENT_STRATEGY="EarlyBlockPlacement"

# FLAGS_CO=-DTAPA_BUFFER_EXPLICIT_RELEASE
# FLAGS_XO=--enable-buffer-exprel --enable-hbm-binding-adjustment --max-search-time 1200 --max-area-limit 0.95 --clock-period 3.33
FLAGS_XO=--enable-hbm-binding-adjustment --max-search-time 1200 --max-area-limit 0.95 --clock-period 2

include ../../utils/common.mk
