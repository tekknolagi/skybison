#!/usr/bin/env python3
import argparse
import hashlib
import json
import logging
import os
import subprocess
import sys

from util import get_repo_root, normalize_revision, run, SCRIPT_PATH


RESULTSDIR = "/var/tmp/benchresults"


def benchmark(repo_root, revision, binary, interpreter_name, resultsdir, args):
    results = []
    if args.run_django:
        cgoutdir = f"{resultsdir}/cg/django"
        run(["mkdir", "-p", cgoutdir])
        cmd = [
            f"{repo_root}/benchmarks/benchmarks/django/benchmark.py",
            "-i",
            binary,
            "--callgrind-out-dir",
            cgoutdir,
            "--json",
            "--callgrind",
        ]
        proc = run(cmd, cwd="/", log_level=logging.INFO)
        results += json.loads(proc.stdout)

    if args.run_benchmarks:
        cgoutdir = f"{resultsdir}/cg"
        cmd = [
            f"{repo_root}/benchmarks/run.py",
            "-i",
            binary,
            "-p",
            f"{repo_root}/benchmarks/benchmarks",
            "--json",
            "--tool=callgrind",
            "--callgrind-out-dir",
            cgoutdir,
        ]
        proc = run(cmd, cwd="/", log_level=logging.INFO)
        results += json.loads(proc.stdout)

    for result in results:
        # Remove interpreter path as it unnecessarily produces
        # differences because of changing temporary directories.
        if "interpreter" in result:
            del result["interpreter"]
        result["interpreter_name"] = interpreter_name
        result["version"] = revision

    return results


def sha256sum(filename):
    h  = hashlib.sha256()
    b  = bytearray(128*1024)
    mv = memoryview(b)
    with open(filename, 'rb', buffering=0) as f:
        for n in iter(lambda : f.readinto(mv), 0):
            h.update(mv[:n])
    return h.hexdigest()


if __name__ == "__main__":
    description = "Checkout, build and benchmark a single revision"
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("revision", nargs="?")
    parser.add_argument(
        "--ignore-cache", dest="check_cache", default=True, action="store_false"
    )
    parser.add_argument(
        "--no-update-cache", dest="update_cache", default=True, action="store_false"
    )
    parser.add_argument(
        "--only-cache", dest="only_cache", default=False, action="store_true"
    )
    parser.add_argument("--run-django", default=None, action="store_true")
    parser.add_argument("--run-benchmarks", default=None, action="store_true")
    parser.add_argument("--use-cpython", default=None)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format="%(message)s", level=log_level)

    repo_root = get_repo_root(SCRIPT_PATH)

    if args.run_django is None and args.run_benchmarks is None:
        args.run_django = True

    cache_env = (
        open(__file__, "r").read()
        + f"django: {args.run_django}"
        + f"benchmarks: {args.run_benchmarks}"
    )
    cache_env_hash = hashlib.sha256(cache_env.encode()).hexdigest()[:8]

    if args.use_cpython is not None:
        revision = sha256sum(args.use_cpython)
        interpreter_name = "cpython"
    else:
        if not args.revision:
            parser.error("Must provide revision to benchmark_rev if not benchmarking CPython")
        revision = normalize_revision(repo_root, args.revision)
        interpreter_name = "pyro"
    resultsdir = f"{RESULTSDIR}/{revision}_{cache_env_hash}"
    cachefile = f"{resultsdir}/result.json"

    results = None
    if args.check_cache:
        try:
            results = json.load(open(cachefile))
            sys.stderr.write(f"Using cached results from {cachefile}\n")
        except Exception:
            pass

    if results is None and not args.only_cache:
        if args.use_cpython:
            binary = args.use_cpython
        else:
            cmd = [f"{SCRIPT_PATH}/build_rev.py"]
            if args.verbose:
                cmd += ["--verbose"]
            cmd += [revision]
            p = run(cmd)
            binary = p.stdout.strip()
            if not binary.startswith("/"):
                logging.critical(f"Build script did not produce a path. Output: {binary}")

        results = benchmark(repo_root, revision, binary, interpreter_name, resultsdir, args)
        if args.update_cache and results:
            try:
                run(["mkdir", "-p", os.path.dirname(cachefile)])
                with open(cachefile, "w") as fp:
                    json.dump(results, fp=fp, indent=2, sort_keys=True)
            except Exception:
                logging.warning(f"Failed to save results to {cachefile}\n")

    out = sys.stdout
    json.dump(results, fp=out, indent=2, sort_keys=True)
    out.write("\n")
