#!/usr/bin/env python3
import argparse
import json
import logging
import subprocess
import sys
from collections import defaultdict

from util import get_repo_root, normalize_revision, run, SCRIPT_PATH


def run_benchmarks(revs, revnames, args):
    rev_samples = []
    for rev in revs:
        cmd = [f"{SCRIPT_PATH}/benchmark_rev.py", *args.benchmark_rev_args, rev]
        proc = run(cmd, stdout=subprocess.PIPE)
        try:
            samples = json.loads(proc.stdout)
            rev_samples.append(samples)
        except Exception:
            sys.stderr.write(f"Failed to parse benchmark data:\n{proc.stdout}\n")
            sys.exit(1)

    return rev_samples


def aggregate_results(rev_samples, revnames):  # noqa: C901
    samples_per_benchmark = defaultdict(list)
    for rev in rev_samples:
        found_benchmarks = set()
        for result in rev:
            if "benchmark" not in result:
                sys.stderr.write("Warning: result without benchmark name\n")
                continue
            benchmark = result["benchmark"]
            if benchmark in found_benchmarks:
                sys.stderr.write("Multiple results for same benchmark?\n")
                continue
            found_benchmarks.add(benchmark)
            samples_per_benchmark[benchmark].append(result)

    results = {}
    for _benchmark, rev_samples in sorted(samples_per_benchmark.items()):
        keys = set()
        for samples in rev_samples:
            keys.update(samples.keys())

        bresults = {}
        for key in keys:
            all_same = True
            common_value = None
            values = []
            for samples in rev_samples:
                value = samples.get(key)
                values.append(value)
                if value is None:
                    continue
                if common_value is None:
                    common_value = value
                elif value != common_value:
                    all_same = False
            if all_same:
                bresults[key] = common_value
                continue
            for revname, value in zip(revnames, values):
                if value is None:
                    continue
                bresults[f"{key} {revname}"] = value

        # Compute delta to rev0
        samples0 = rev_samples[0]
        for key, value0 in samples0.items():
            if key in bresults:
                continue
            if not isinstance(value0, (float, int)) or value0 == 0:
                continue
            for revname, samples in zip(revnames[1:], rev_samples[1:]):
                if key not in samples:
                    continue
                value1 = samples[key]
                delta_key = f"{key} ∆" + ("" if len(revnames) == 2 else f" {revname}")
                rel_delta = (value1 - value0) / float(value0)
                bresults[delta_key] = f"{rel_delta * 100.:2.1f}%"
        results[bresults["benchmark"]] = bresults
    return results


if __name__ == "__main__":
    description = "Checkout, build, benchmark and compare multiple revisions"
    epilog = f"""

Example:
    {sys.argv[0]} .~1 .              # compare parent with current revision
"""
    parser = argparse.ArgumentParser(description=description, epilog=epilog)
    parser.add_argument("revisions", metavar="REVISION", nargs="+", default="")
    parser.add_argument(
        "--run-django",
        dest="benchmark_rev_args",
        default=[],
        action="append_const",
        const="--run-django",
    )
    parser.add_argument(
        "--run-benchmarks",
        dest="benchmark_rev_args",
        action="append_const",
        const="--run-benchmarks",
    )
    parser.add_argument(
        "--ignore-cache",
        dest="benchmark_rev_args",
        action="append_const",
        const="--ignore-cache",
    )
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format="%(message)s", level=log_level, stream=sys.stderr)

    if args.verbose:
        args.benchmark_rev_args.insert(0, "--verbose")

    if len(args.revisions) < 2:
        parser.print_help()
        sys.stdout.write("\n\nError: Need at least two revision numbers\n")
        sys.exit(1)

    repo_root = get_repo_root(SCRIPT_PATH)

    revs = args.revisions
    if len(revs) == 2:
        revnames = ["before", "now"]
    else:
        revnames = [str(x) for x in range(len(revs))]
    revs = [normalize_revision(repo_root, rev) for rev in revs]

    rev_samples = run_benchmarks(revs, revnames, args)

    results = aggregate_results(rev_samples, revnames)

    out = sys.stdout
    json.dump(results, fp=out, indent=2, sort_keys=True, ensure_ascii=False)
    out.write("\n")