import math
import logging

logger = logging.getLogger().getChild(__name__)

# project config
KERNEL = None
KERNEL_TOP = None
KERNEL_ARGS = None
PLATFORM = "xilinx_u200_gen3x16_xdma_2_202110_1"
BUILD_DIR_PREFIX = None
XRT_INI_PATH = "scripts/xrt.ini"
MAX_SYNTH_JOBS = 8
STRATEGY = "Explore"
PLACEMENT_STRATEGY= "EarlyBlockPlacement"

### BRAHMA
# arch
SB_NXCTRS = 8
SB_NXCSRS = 8
SB_NXCSRS_LOG2 = math.ceil(math.log2(SB_NXCSRS))
SB_NRX = SB_NXCTRS
SB_NTX = SB_NXCTRS
SB_NUNUSED = 1
SB_NDEBUGQS = 0
SB_BURST_SIZE = 32
SB_XCTRQ_DEPTH = SB_BURST_SIZE
SB_DQS_DEPTH = SB_BURST_SIZE
SB_CQS_DEPTH = 2
SB_RARBQS_DEPTH = 2
SB_WARBQS_DEPTH = 2

# user
SB_NUM_PAGES = 8
SB_WORD_SIZE = 4
SB_WORD_SIZE_BITS = SB_WORD_SIZE << 3
SB_PAGE_SIZE = 1024
SB_MSGS_PER_PAGE = 32

def makeIdentity():
    global IDENTITY
    IDENTITY = str(PLATFORM) + "/"\
            + "nxctrs" + str(SB_NXCTRS)     \
            + "bs" + str(SB_BURST_SIZE)     \
            + "pages" + str(SB_NUM_PAGES)   \
            + "mpp" + str(SB_MSGS_PER_PAGE)
    return IDENTITY

def checkSanity():
    sane = True
    if(SB_XCTRQ_DEPTH < SB_BURST_SIZE):
        logger.warning(f"XCTRQ_DEPTH ({SB_XCTRQ_DEPTH}) is less than BURST_SIZE ({SB_BURST_SIZE})")
        sane = False
    return sane

def writeMakefile(workdir, commonmkfilepath):
    with open(workdir+"Makefile", "w") as makefile:
        makefile.write(f"SHELL:=/bin/bash\n")
        makefile.write(f"KERNEL={KERNEL}\n")
        makefile.write(f"KERNEL_TOP={KERNEL_TOP}\n")
        makefile.write(f"PLATFORM={PLATFORM}\n")
        makefile.write(f"BUILD_DIR_PREFIX={BUILD_DIR_PREFIX}\n\n")
        makefile.write(f"XRT_INI_PATH={XRT_INI_PATH}\n")
        makefile.write(f"MAX_SYNTH_JOBS={MAX_SYNTH_JOBS}\n")
        if(KERNEL_ARGS is not None):
            makefile.write(f"KERNEL_ARGS={KERNEL_ARGS}\n")
        if(STRATEGY is not None):
            makefile.write(f"STRATEGY={STRATEGY}\n")
        if(PLACEMENT_STRATEGY is not None):
            makefile.write(f"PLACEMENT_STRATEGY={PLACEMENT_STRATEGY}\n")
        makefile.write(f"\ninclude {commonmkfilepath}\n")
    logger.info("Generated Makefile")

# write to parameter file
def writeSBConfig(workdir):
    HEADER = "#ifndef __SB_CONFIG_H__\n#define __SB_CONFIG_H__\n"
    with open(workdir+"sb_config.h", "w") as headerFile:
        headerFile.write(HEADER)
        headerFile.write("\n")
        headerFile.write("#include \"tapa.h\"\n")
        headerFile.write("#include \"ap_int.h\"\n")
        headerFile.write("\n\n// arch")
        headerFile.write(f"\n#define SB_NXCTRS         ({SB_NXCTRS})")
        headerFile.write(f"\n#define SB_NXCSRS         ({SB_NXCSRS})")
        headerFile.write(f"\n#define SB_NXCSRS_LOG2    ({SB_NXCSRS_LOG2})")
        headerFile.write(f"\n#define SB_NRX            ({SB_NRX})")
        headerFile.write(f"\n#define SB_NTX            ({SB_NTX})")
        headerFile.write(f"\n#define SB_NDEBUGQS       ({SB_NDEBUGQS})")
        headerFile.write(f"\n#define SB_BURST_SIZE     ({SB_BURST_SIZE})")
        headerFile.write(f"\n#define SB_XCTRQ_DEPTH    ({SB_XCTRQ_DEPTH})")
        headerFile.write(f"\n#define SB_DQS_DEPTH      ({SB_DQS_DEPTH})")
        headerFile.write(f"\n#define SB_CQS_DEPTH      ({SB_CQS_DEPTH})")
        headerFile.write(f"\n#define SB_RARBQS_DEPTH   ({SB_RARBQS_DEPTH})")
        headerFile.write(f"\n#define SB_WARBQS_DEPTH   ({SB_WARBQS_DEPTH})")
        headerFile.write(f"\n\n// user")
        headerFile.write(f"\n#define SB_NUM_PAGES      ({SB_NUM_PAGES})")
        headerFile.write(f"\n#define SB_WORD_SIZE      ({SB_WORD_SIZE})")
        headerFile.write(f"\n#define SB_WORD_SIZE_BITS ({SB_WORD_SIZE_BITS})")
        headerFile.write(f"\n#define SB_PAGE_SIZE      ({SB_PAGE_SIZE})")
        headerFile.write(f"\n#define SB_MSGS_PER_PAGE  ({SB_MSGS_PER_PAGE})")
        headerFile.write(f"\n\n#endif // __SB_CONFIG_H__\n")
    logging.info("Generated sb_config.h")