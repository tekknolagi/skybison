#!/usr/bin/env python3
import argparse
import hashlib
import logging
import os
import shutil
import subprocess
import sys
import tempfile

from util import get_repo_root, normalize_revision, run, SCRIPT_PATH


KEEP_BUILDS = 15
BUILDS_DIR = "/var/tmp/bench_builds"


def find_binary(builddir):
    candidates = (
        f"{builddir}/bin/python",  # new revisions
        f"{builddir}/python",  # older revisions
    )
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    return None


def ccache_flags():
    ccache = shutil.which("ccache")
    if not ccache:
        return []
    return [
        f"-DCMAKE_C_COMPILER_LAUNCHER=ccache",
        f"-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
    ]


def compiler_flags():
    clang = shutil.which("clang")
    if not clang:
        return []
    return [
        f"-DCMAKE_C_COMPILER=clang",
        f"-DCMAKE_CXX_COMPILER=clang++",
    ]


def build(repo_root, builddir, sourcedir):
    cmake_flags = [
        "-S",
        sourcedir,
        "-B",
        builddir,
        f"-DCMAKE_TOOLCHAIN_FILE={sourcedir}/util/linux.cmake",
        "-DCMAKE_BUILD_TYPE=Release",
        *ccache_flags(),
        *compiler_flags(),
    ]
    run(
        ["cmake", "-GNinja"] + cmake_flags,
        log_level=logging.INFO,
    )
    run(
        ["ninja", "-C", builddir, "python"],
        log_level=logging.INFO,
    )

    binary = find_binary(builddir)
    if binary is None:
        logging.critical(f"No python binary found in {builddir}")
        sys.exit(1)
    return binary


def checkout_and_build(repo_root, revision, builddir):
    with tempfile.TemporaryDirectory(prefix="bench_") as tempdir:
        workdir = f"{tempdir}/workdir"
        run(
            ["git", "worktree", "add", "--detach", workdir, revision],
            cwd=repo_root,
        )
        try:
            run(["mkdir", "-p", builddir], log_level=logging.DEBUG)
            return build(repo_root, builddir, workdir)
        finally:
            run(["git", "worktree", "remove", "-f", workdir])


def clean_old_builds():
    builds = []
    for entry in os.scandir(BUILDS_DIR):
        stat = entry.stat()
        builds.append((stat.st_mtime, entry.path))
    builds.sort(reverse=True)
    for _, old_build in builds[KEEP_BUILDS - 1 :]:
        run(["rm", "-rf", old_build], log_level=logging.INFO)


if __name__ == "__main__":
    description = "Checkout and build a single revision"
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--no-clean", action="store_true")
    parser.add_argument("--no-cache", action="store_true")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("revision")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format="%(message)s", level=log_level)

    repo_root = get_repo_root(SCRIPT_PATH)

    cache_env = open(__file__, "r").read()
    cache_env_hash = hashlib.sha256(cache_env.encode()).hexdigest()[:8]

    run(["mkdir", "-p", BUILDS_DIR], log_level=logging.DEBUG)

    revision = normalize_revision(repo_root, args.revision)
    builddir = f"{BUILDS_DIR}/{revision}_{cache_env_hash}"
    binary = None
    if os.path.exists(builddir):
        if args.no_cache:
            run(["rm", "-rf", builddir], log_level=logging.INFO)
        else:
            binary = find_binary(builddir)
            if binary is not None:
                run(["touch", builddir])
                print(binary)
                sys.exit(0)
            # Looks like an invalid/incomplete build, remove it.
            run(["rm", "-rf", builddir], log_level=logging.INFO)

    if not args.no_clean:
        clean_old_builds()

    if binary is None:
        binary = checkout_and_build(repo_root, revision, builddir)

    print(binary)
