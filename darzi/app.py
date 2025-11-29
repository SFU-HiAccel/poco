import helpers
import config_gen
import logging
import os
import sys
from multiprocessing import Pool

logger = logging.getLogger(__name__)

SIM_WS = os.environ.get('SIM_WS')
GFS_WS = os.environ.get('GFS_WS')

# project name
PROJ_NAME = "par_read_noc"
# where is the common Makefile?
COMMONMK = GFS_WS+"sb/tapa/shared-buffer/brahma_link/brahma/utils/common.mk"
# pick folder
SOURCE_FOLDER = GFS_WS+"sb/tapa/shared-buffer/brahma_link/brahma/tests/"
# get top-level build directory for all configs
TOP_BUILD_DIR = SIM_WS+"builds/brahma/sweep/"

# copy sources to TOP_BUILD_DIR
helpers.cmdLineWaitUntilExecution(f"rm -rf {TOP_BUILD_DIR+PROJ_NAME}/project_source")
helpers.cmdLineWaitUntilExecution(f"cp -r {SOURCE_FOLDER+PROJ_NAME} {TOP_BUILD_DIR+PROJ_NAME}/project_source")

# generate all Makefiles in a parallel directory
# helpers.cmdLineWaitUntilExecution(f"mkdir -p {TOP_BUILD_DIR}/Makefiles")

config_gen.KERNEL="add"
config_gen.KERNEL_TOP="vecadd"
config_gen.STRATEGY=None
config_gen.PLACEMENT_STRATEGY=None
config_gen.SB_NUNUSED=4

BUILD_IDENTITIES = []

for burst_size in [8, 16, 32]:
    for pages in [4, 8, 16]:
        for mpp in [128, 256, 512, 1024]:
            config_gen.SB_BURST_SIZE = burst_size
            config_gen.SB_NUM_PAGES = pages
            config_gen.SB_MSGS_PER_PAGE = mpp
            identity = config_gen.makeIdentity()
            print(identity)
            config_gen.checkSanity()
            logger.info(f"Working on Identity:{identity}")
            config_gen.BUILD_DIR_PREFIX = TOP_BUILD_DIR + PROJ_NAME + "/" + identity + "/"
            BUILD_IDENTITIES.append("/mnt/glusterfs/users/arb26/workspace/sb/tapa/shared-buffer/brahma_link/brahma/sweep/"+PROJ_NAME+"/"+identity+"/")
            # create the build directory for this config and nuke it
            helpers.cmdLineWaitUntilExecution(f"mkdir -p {config_gen.BUILD_DIR_PREFIX}")
            helpers.cmdLineWaitUntilExecution(f"rm -rf {config_gen.BUILD_DIR_PREFIX}/*")
            # link everything from copied source folder into the build directory for this config
            helpers.cmdLineWaitUntilExecution(f"ln -sfn {TOP_BUILD_DIR+PROJ_NAME}/project_source/* {config_gen.BUILD_DIR_PREFIX}")
            # remove links for anything we are going to generate automatically
            for linkname in ['inc', 'Makefile', 'builds']:
                helpers.cmdLineWaitUntilExecution(f"rm -f {config_gen.BUILD_DIR_PREFIX}/{linkname}")
            # generate stuff automatically
            helpers.cmdLineWaitUntilExecution(f"mkdir -p {config_gen.BUILD_DIR_PREFIX}/inc")
            config_gen.writeSBConfig(workdir=config_gen.BUILD_DIR_PREFIX+"/inc/")
            config_gen.writeMakefile(workdir=config_gen.BUILD_DIR_PREFIX, commonmkfilepath=COMMONMK)

mktarget = 'hw'
def do_build(directory):
    print(f"Building {mktarget} for {os.path.basename(os.path.normpath(directory))}")
    helpers.cmdLineWaitUntilExecution(f"cd {directory} && make {mktarget}")

with Pool(processes=8) as pool:
    pool.map(do_build, iter(BUILD_IDENTITIES))
