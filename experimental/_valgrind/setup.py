#!/usr/bin/env python3
import os
import shlex
import sysconfig
from distutils.core import Extension, setup


# Hack for python runtimes built in TP2...
if "/home/engshare" in sysconfig.get_config_var("CC"):
    if "CC" not in os.environ:
        os.environ["CC"] = "gcc.par -pthread"
    if "CXX" not in os.environ:
        os.environ["CXX"] = "g++.par -pthread"
    if "LDSHARED" not in os.environ:
        ldshared = shlex.split(sysconfig.get_config_var("LDSHARED"))
        ldshared[0] = "gcc.par"
        os.environ["LDSHARED"] = shlex.join(ldshared)


module1 = Extension(
    "_valgrind",
    sources=["_valgrind.c"],
    library_dirs=["/usr/local/fbcode/platform009/lib"],
    include_dirs=["/usr/local/fbcode/platform009/include"],
)

setup(
    name="_valgrind",
    version="1.0",
    description="valgrind helper package",
    ext_modules=[module1],
)
