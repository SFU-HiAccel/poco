import subprocess

CAPTURE_OUTPUT = False

def printChild():
    global CAPTURE_OUTPUT
    CAPTURE_OUTPUT = False

def cmdLine(cmd):
    process = subprocess.run(args = cmd, capture_output = CAPTURE_OUTPUT, universal_newlines = True, shell = True)
    # process = subprocess.run(args = cmd, stdout = None, stderr = None, universal_newlines = True, shell = True, capture_output=True)
    return process

def cmdLineThread(cmd, wait):
    process = subprocess.Popen(args = cmd, universal_newlines = True, shell = True)
    if(wait):
        while(process.poll() == None):
            continue

def cmdLineWaitUntilExecution(cmd):
    process = subprocess.run(args = cmd, capture_output = CAPTURE_OUTPUT, universal_newlines = True, shell = True)
    return process