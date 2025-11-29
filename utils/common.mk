SHELL:=/bin/bash
POCO_ROOT=${WORKSPACE}/poco/

ifndef KERNEL
KERNEL=add
endif

ifndef KERNEL_TOP
KERNEL_TOP=VecAdd
endif

ifndef KERNEL_ARGS
KERNEL_ARGS=
endif

ifndef PLATFORM
PLATFORM=xilinx_u200_gen3x16_xdma_2_202110_1
endif

ifndef FLOORPLAN_STRATEGY
FLOORPLAN_STRATEGY=HALF_SLR_LEVEL_FLOORPLANNING
endif

ifndef XRT_INIT_PATH
XRT_INI_PATH=scripts/xrt.ini
endif

ifndef MAX_SYNTH_JOBS
MAX_SYNTH_JOBS=40
endif

ifndef ENV_ROUTE_DESIGN_STRATEGY
ENV_ROUTE_DESIGN_STRATEGY="Explore"
ENV_OPT_DESIGN_STRATEGY="Explore"
ENV_PHYS_OPT_DESIGN_STRATEGY="Explore"
endif

ifndef ENV_PLACE_DESIGN_STRATEGY
ENV_PLACE_DESIGN_STRATEGY="EarlyBlockPlacement"
endif

KERNEL_CO=${BUILD_DIR_PREFIX}/${KERNEL_TOP}
KERNEL_XO=${BUILD_DIR_PREFIX}/${KERNEL_TOP}.${PLATFORM}.hw.xo
KERNEL_XCLBIN_EM=${BUILD_DIR_PREFIX}/vitis_run_hw_emu/${KERNEL_TOP}_${PLATFORM}.hw_emu.xclbin
KERNEL_XCLBIN_HW=${BUILD_DIR_PREFIX}/vitis_run_hw/${KERNEL_TOP}_${PLATFORM}.xclbin
RUNXOVDBG_OUTPUT_DIR="${BUILD_DIR_PREFIX}/vitis_run_hw_emu"
VPP_TEMP_DIR="${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_TOP}_${PLATFORM}.temp"
TOP_HIER=level0_i:level0_i/ulp/${KERNEL_TOP}/inst/${KERNEL_TOP}_inner_0

.PHONY: all c xo runxo runxodbg runxov runxovdbg hw runhw cleanall clean cleanxo cleanhdl

all: c xo hw

################
### SW_EMU
################
c: ${KERNEL_CO}
	-${KERNEL_CO} ${KERNEL_ARGS}

pococ:
	../../darzi/poco.py --src src/${KERNEL}_poco.cpp --dest src/${KERNEL}.cpp --adir ../../darzi/brahma_assets/mpphd_ap_wide/ --top ${KERNEL} --buffer_config mpmc_report.json --clang-ipath "/usr/local/include" --clang-ipath "/usr/lib/gcc/x86_64-linux-gnu/7.5.0/include" --clang-ipath "/usr/include/linux" --clang-ipath "/usr/include" --clang-ipath "${XILINX_HLS}/include" --clang-ipath "../../darzi"
	mkdir -p ${BUILD_DIR_PREFIX}
	@echo "[MAKE]: Compiling for C target"
	g++ -g -o ${KERNEL_CO} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -I${WORKSPACE}/sblocal/tapa/src/tapa -ltapa -lfrt -lglog -lgflags -lOpenCL -std=c++17 -DTAPA_BUFFER_SUPPORT ${FLAGS_CO}
	-${KERNEL_CO} ${KERNEL_ARGS}

${KERNEL_CO}: src/${KERNEL}.cpp src/${KERNEL}-host.cpp src/${KERNEL}.h
	mkdir -p ${BUILD_DIR_PREFIX}
	@echo "[MAKE]: Compiling for C target"
	g++ -g -o ${KERNEL_CO} -O2 src/${KERNEL}.cpp src/${KERNEL}-host.cpp -I${XILINX_HLS}/include -I${WORKSPACE}/sblocal/tapa/src/tapa -ltapa -lfrt -lglog -lgflags -lOpenCL -std=c++17 -DTAPA_BUFFER_SUPPORT ${FLAGS_CO}


################
### HW_EMU
################

xo: ${KERNEL_XO}
	@echo "[MAKE]: Looking for prebuilt XO"

qxo: ${KERNEL_CO} 
	@echo "[MAKE]: Compiling for XO target"
	tapac -v 1 -o ${KERNEL_XO} src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL_XO}.tapa --enable-buffer-support --connectivity system.cfg --max-parallel-synth-jobs 24 --separate-complex-buffer-tasks ${FLAGS_XO} --quick-impl
	mkdir -p ${BUILD_DIR_PREFIX}/reports

${KERNEL_XO}: ${KERNEL_CO}
	@echo "[MAKE]: Compiling for XO target"
#	tapac -vv -o ${KERNEL_XO} src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL_XO}.tapa --enable-buffer-support --connectivity system.cfg --max-parallel-synth-jobs 24 --separate-complex-buffer-tasks ${FLAGS_XO}
	tapac -v 1 -o ${KERNEL_XO} src/${KERNEL}.cpp --platform ${PLATFORM} --top ${KERNEL_TOP} --work-dir ${KERNEL_XO}.tapa --enable-buffer-support --connectivity system.cfg --max-parallel-synth-jobs 24 --separate-complex-buffer-tasks --floorplan-output ${BUILD_DIR_PREFIX}/constraint.tcl ${FLAGS_XO} --floorplan-strategy ${FLOORPLAN_STRATEGY}
	echo "" >> ${BUILD_DIR_PREFIX}/constraint.tcl
	echo "" >> ${BUILD_DIR_PREFIX}/constraint.tcl
	mkdir -p ${BUILD_DIR_PREFIX}/reports
	echo "report_accelerator_utilization -kernels { level0_i:level0_i/ulp/${KERNEL_TOP}/inst/${KERNEL_TOP}_inner_0:${KERNEL_TOP} } -file ${BUILD_DIR_PREFIX}/reports/accl_synth_util.rpt -name accl_synth_util -json" >> ${BUILD_DIR_PREFIX}/constraint.tcl

runxo: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Running HW_EMU (.xo)"
	@cd ${BUILD_DIR_PREFIX}
	${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XO}

runxodbg: xo
	@echo "[MAKE]: Target waveform"
	@echo "[MAKE]: Running waveform for HW_EMU (.xo)"
	@cd ${BUILD_DIR_PREFIX}
	-${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XO} -xosim_work_dir ${BUILD_DIR_PREFIX}/xosim -xosim_save_waveform
	@head -n -2 ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim.tcl > ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl
	@echo "open_wave_config {${BUILD_DIR_PREFIX}/wave.wcfg}" >> ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl
	vivado -mode gui -source ${BUILD_DIR_PREFIX}/xosim/output/run/run_cosim_no_exit.tcl

runxov: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	@cd ${BUILD_DIR_PREFIX}
	v++ -o ${KERNEL_XCLBIN_EM} \
	--link \
	--target hw_emu\
  --kernel ${KERNEL_TOP} \
	--platform ${PLATFORM} \
	--temp_dir ${VPP_TEMP_DIR} \
	--log_dir ${VPP_TEMP_DIR}/logs \
	--report_dir ${VPP_TEMP_DIR}/reports \
	${KERNEL_XO}
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	@cd ${BUILD_DIR_PREFIX}
	${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XCLBIN_EM}

runxovdbg: xo
	@echo "[MAKE]: Target HW_EMU"
	@echo "[MAKE]: Building .xclbin through Vitis"
	@cp system.cfg ${BUILD_DIR_PREFIX}/system.cfg
	export XRT_INI_PATH=${PWD}/scripts/xrt.ini
	@cd ${BUILD_DIR_PREFIX} && \
	v++ -g \
	--config ${BUILD_DIR_PREFIX}/system.cfg \
	--link \
	--output "${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_XCLBIN_EM}" \
	--kernel ${KERNEL_TOP} \
	--platform ${PLATFORM} \
	--target hw_emu \
	--report_level 2 \
	--temp_dir ${VPP_TEMP_DIR} \
	--log_dir ${VPP_TEMP_DIR}/logs \
	--report_dir ${VPP_TEMP_DIR}/reports \
	--optimize 3 \
	--connectivity.nk ${KERNEL_TOP}:1:${KERNEL_TOP} \
	--save-temps \
	"${KERNEL_XO}" \
	--vivado.synth.jobs ${MAX_SYNTH_JOBS} \
	--vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \
	--vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=Explore \
	--vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=EarlyBlockPlacement \
	--vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=Explore \
	--vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=Explore \
	@echo "[MAKE]: Running HW_EMU (.xclbin)"
	-${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${RUNXOVDBG_OUTPUT_DIR}/${KERNEL_XCLBIN_EM}
	xsim -gui *.wdb



################
### HARDWARE
################
hw: ${KERNEL_XCLBIN_HW}

${KERNEL_XCLBIN_HW}: xo
	@echo "[MAKE]: Building HW target"
	#export ENV_OPT_DESIGN_STRATEGY=${ENV_OPT_DESIGN_STRATEGY}
	#export ENV_PLACE_DESIGN_STRATEGY=${ENV_PLACE_DESIGN_STRATEGY}
	#export ENV_ROUTE_DESIGN_STRATEGY=${ENV_ROUTE_DESIGN_STRATEGY}
	#export ENV_PHYS_OPT_DESIGN_STRATEGY=${ENV_PHYS_OPT_DESIGN_STRATEGY}
	cd ${BUILD_DIR_PREFIX} && source ${BUILD_DIR_PREFIX}/${KERNEL_TOP}.${PLATFORM}.hw_generate_bitstream.sh
	vivado -mode tcl -source ${BUILD_DIR_PREFIX}/report_impl_util.tcl
	python3 ${POCO_ROOT}/scripts/build_metrics/app.py --bdir ${BUILD_DIR_PREFIX} --log ${BUILD_DIR_PREFIX}/reports/bm.log

runhw: ${KERNEL_XCLBIN_HW}
	@echo "[MAKE]: Target HW"
	@echo "[MAKE]: Running HW (.xclbin)"
	cd ${BUILD_DIR_PREFIX} && ${KERNEL_CO} ${KERNEL_ARGS} --bitstream=${KERNEL_XCLBIN_HW}

### CLEAN
cleanall: clean cleanxo cleandbg cleanhw
	@echo "[MAKE]: Cleaned everything"

cleanhw:
	@echo "[MAKE]: Cleaning HW in ${BUILD_DIR_PREFIX}/vitis_run_hw"
	rm -rf ${BUILD_DIR_PREFIX}/vitis_run_hw

clean:
	@echo "[MAKE]: Cleaning C artefacts"
	rm -f ${KERNEL_CO}

cleanxo:
	@echo "[MAKE]: Cleaning XO artefacts"
	rm -f ${KERNEL_XCLBIN_EM}
	rm -f ${KERNEL_XO}
	rm -f ${BUILD_DIR_PREFIX}/${KERNEL_TOP}.${PLATFORM}.hw_generate_bitstream.sh
	rm -rf ${KERNEL_XO}.tapa

cleandbg:
	@echo "[MAKE]: Cleaning debug artefacts in ${BUILD_DIR_PREFIX}"
# delete all logs
	rm -f ${BUILD_DIR_PREFIX}/*.jou
	rm -f ${BUILD_DIR_PREFIX}/*.log
	rm -f ${BUILD_DIR_PREFIX}/*.run_summary
# delete bloat
	rm -rf ${BUILD_DIR_PREFIX}/vivado
	rm -rf ${BUILD_DIR_PREFIX}/xosim
	rm -rf ${BUILD_DIR_PREFIX}/vitis_run_hw_emu
	rm -rf ${BUILD_DIR_PREFIX}/*.protoinst
	rm -rf ${BUILD_DIR_PREFIX}/xsim.dir
# only delete wave configurations that were automatically generated	
	rm -rf ${BUILD_DIR_PREFIX}/*${PLATFORM}*.wcfg
	rm -f  ${BUILD_DIR_PREFIX}/constraint.tcl
	#rm -f  ${KERNEL_XO}
	#rm -rf ${KERNEL_XO}.tapa
# delete some intermediary files generated from the custom scripts
	cd ${BUILD_DIR_PREFIX}
	rm -f opencl_trace.csv profile_kernels.csv summary.csv timeline_kernels.csv runs.md

cleanrtl:
	@echo "[MAKE]: Cleaning RTL artefacts at ${KERNEL_XO}.tapa/hdl/"
	rm -rf ${KERNEL_XO}.tapa/hdl/
