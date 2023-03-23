import logging
import os
import subprocess
import sys
import shlex
import textwrap

SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))


def run(
    cmd,
    verbose=True,
    cwd=None,
    check=True,
    capture_output=False,
    encoding="utf-8",
    log_level=logging.DEBUG,
    # Only pipe stdout; we want stderr (logging, errors) to be unbuffered.
    stdout=subprocess.PIPE,
    # Specify an integer number of seconds
    timeout=-1,
    **kwargs,
):
    if verbose:
        info = "$ "
        if cwd is not None:
            info += f"cd {cwd}; "
        info += " ".join(shlex.quote(c) for c in cmd)
        if capture_output:
            info += " >& ..."
        lines = textwrap.wrap(
            info,
            break_on_hyphens=False,
            break_long_words=False,
            replace_whitespace=False,
            subsequent_indent="  ",
        )
        logging.log(log_level, " \\\n".join(lines))
    if timeout != -1:
        cmd = ["timeout", "--signal=KILL", f"{timeout}s", *cmd]
    try:
        return subprocess.run(
            cmd,
            cwd=cwd,
            check=check,
            capture_output=capture_output,
            encoding=encoding,
            stdout=stdout,
            **kwargs,
        )
    except subprocess.CalledProcessError as e:
        if e.returncode == -9:
            # Error code from `timeout` command signaling it had to be killed
            raise TimeoutError("Command timed out", cmd)
        logging.critical("%s", e.stdout)
        raise


def get_repo_root(dirname):
    proc = run(
        ["git", "rev-parse", "--show-toplevel"],
        cwd=dirname,
    )
    return proc.stdout.strip()


def normalize_revision(repo_root, revspec):
    proc = run(
        ["git", "rev-parse", revspec],
        cwd=repo_root,
    )
    return proc.stdout.strip()
