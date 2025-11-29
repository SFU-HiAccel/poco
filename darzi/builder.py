import helpers

def do_build(directory, mktarget):
    helpers.cmdLineWaitUntilExecution(f"cd {directory} && make {mktarget}")