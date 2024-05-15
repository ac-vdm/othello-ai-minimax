import subprocess
import os
import threading
import time
import json
import uuid
import re

def run_command(command, print_output=False):
    subproc = subprocess.Popen(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    if print_output:
      output = ""
      while subproc.poll() is None:
          line = subproc.stdout.readline()
          output += line.decode()
          if line:
              print(line.decode().strip())
    else:
      output = subproc.stdout.read().decode()
    error = subproc.stderr.read().decode()
    exitCode = subproc.wait()
    subproc.kill()
    return output, error, exitCode


def rm_logs():
    run_command("rm black*.txt")
    run_command("rm white*.txt")
    os.chdir("Logs")
    run_command("rm *.log")
    run_command("rm *.txt")
    os.chdir("..")

if __name__=="__main__":
    print("Removing log files...")
    rm_logs()
    print("Done")
