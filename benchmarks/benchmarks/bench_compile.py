#!/usr/bin/env python3

import argparse
import os
from compileall import compile_file

BENCHMARK_DIR = os.path.dirname(__file__)
FILES = tuple(
    os.path.join(BENCHMARK_DIR, file)
    for file in (
        "2to3.py",
        "bench_base64.py",
        "bench_pickle.py",
        "deltablue.py",
        "fannkuch.py",
        "go.py",
        "loadproperty.py",
        "nbody.py",
        "nqueens.py",
        "pyflate.py",
        "pystone.py",
        "richards.py",
    )
)


def compile_modules(n):
    for _ in range(n):
        for file in FILES:
            # Ignore cached bytecode
            result = compile_file(file, force=True, quiet=True)
            assert result


def run():
    compile_modules(1)


def warmup():
    compile_modules(1)


def jit():
    try:
        from _builtins import _jit_fromtype

        _jit(compile_modules)
    except ImportError:
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "num_iterations",
        type=int,
        default=1,
        nargs="?",
        help="Number of iterations to run the benchmark",
    )
    parser.add_argument("--jit", action="store_true", help="Run in JIT mode")
    args = parser.parse_args()
    warmup()
    if args.jit:
        jit()

    compile_modules(args.num_iterations)
