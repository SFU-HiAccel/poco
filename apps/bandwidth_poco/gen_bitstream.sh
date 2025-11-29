#!/bin/bash
TARGET=hw
# TARGET=hw_emu
# DEBUG=-g

TOP=bfs
XO='/localhdd/arb26/workspace/builds/brahma/bfs/bfs.xilinx_u280_gen3x16_xdma_1_202211_1.hw.xo'
CONSTRAINT='/localhdd/arb26/workspace/builds/brahma/bfs/constraint.tcl'

# check that the floorplan tcl exists
if [ ! -f "$CONSTRAINT" ]; then
    echo "no constraint file found"
    exit
fi

CONFIG_FILE='/localhdd/arb26/workspace/brahmalocal/brahma/tests/darzi_delta_wide/system.cfg'
TARGET_FREQUENCY=300
PLATFORM=xilinx_u280_gen3x16_xdma_1_202211_1
OUTPUT_DIR="$(pwd)/vitis_run_${TARGET}"

MAX_SYNTH_JOBS=24
LOGIC_OPT_STRATEGY="ExploreWithRemap"
PHYS_OPT_STRATEGY="AggressiveExplore"
ROUTING_STRATEGY="AggressiveExplore"
PLACEMENT_STRATEGY="WLDrivenBlockPlacement"

v++ ${DEBUG} \
  --link \
  --output "${OUTPUT_DIR}/${TOP}_${PLATFORM}.xclbin" \
  --kernel ${TOP} \
  --platform ${PLATFORM} \
  --target ${TARGET} \
  --report_level 2 \
  --temp_dir "${OUTPUT_DIR}/${TOP}_${PLATFORM}.temp" \
  --optimize 3 \
  --connectivity.nk ${TOP}:1:${TOP} \
  --save-temps \
  "${XO}" \
  --vivado.synth.jobs ${MAX_SYNTH_JOBS} \
  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \
  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=$LOGIC_OPT_STRATEGY \
  --vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=$PLACEMENT_STRATEGY \
  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=$PHYS_OPT_STRATEGY \
  --vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=$ROUTING_STRATEGY \
  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=$CONSTRAINT \
  --config "${CONFIG_FILE}" \
  --kernel_frequency ${TARGET_FREQUENCY} \
