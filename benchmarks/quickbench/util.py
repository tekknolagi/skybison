import logging
import os
import subprocess
import sys


SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))


def run(cmd, check=True, encoding="utf-8", log_level=logging.DEBUG, **kwargs):
    if "cwd" in kwargs:
        log_msg = f"$ cd {kwargs['cwd']}; {' '.join(cmd)}"
    else:
        log_msg = f"$ {' '.join(cmd)}"
    logging.log(log_level, log_msg)
    return subprocess.run(cmd, check=check, encoding=encoding, **kwargs)


def get_repo_root(dirname):
    completed_process = run(
        ["hg", "root"], check=False, cwd=dirname, stdout=subprocess.PIPE
    )
    if completed_process.returncode == 0:
        return completed_process.stdout.strip()

    sys.stderr.write("Unknown source control\n")
    sys.exit(1)


def normalize_revision(repo_root, revspec):
    proc = run(
        ["hg", "log", "-r", revspec, "--template", "{node}"],
        stdout=subprocess.PIPE,
        cwd=repo_root,
    )
    return proc.stdout.strip()