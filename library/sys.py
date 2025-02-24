#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
# $builtin-init-module$

from builtins import SimpleNamespace as _SimpleNamespace

from _builtins import (
    _builtin,
    _get_asyncgen_hooks,
    _int_check,
    _structseq_new_type,
    _Unbound,
    _unimplemented,
)
from _builtins import maxunicode  # noqa: F401
from _io import TextIOWrapper
from _path import dirname as _dirname, join as _join


# These values are all injected by our boot process. flake8 has no knowledge
# about their definitions and will complain without these lines.
_stderr_fd = _stderr_fd  # noqa: F821
_stdin_fd = _stdin_fd  # noqa: F821
_stdout_fd = _stdout_fd  # noqa: F821
_version_releaselevel = _version_releaselevel  # noqa: F821
hexversion = hexversion  # noqa: F821
_use_buffered_stdio = _use_buffered_stdio  # noqa: F821


_Flags = _structseq_new_type(
    "sys.flags",
    (
        "debug",
        "inspect",
        "interactive",
        "optimize",
        "dont_write_bytecode",
        "no_user_site",
        "no_site",
        "ignore_environment",
        "verbose",
        "bytes_warning",
        "quiet",
        "hash_randomization",
        "isolated",
        "dev_mode",
        "utf8_mode",
    ),
    is_heaptype=False,
)


_FloatInfo = _structseq_new_type(
    "sys.float_info",
    (
        "max",
        "max_exp",
        "max_10_exp",
        "min",
        "min_exp",
        "min_10_exp",
        "dig",
        "mant_dig",
        "epsilon",
        "radix",
        "rounds",
    ),
    is_heaptype=False,
)


_HashInfo = _structseq_new_type(
    "sys.hash_info",
    (
        "width",
        "modulus",
        "inf",
        "nan",
        "imag",
        "algorithm",
        "hash_bits",
        "seed_bits",
        "cutoff",
    ),
    is_heaptype=False,
)

_AsyncGenHooks = _structseq_new_type(
    "sys.asyncgen_hooks", ("firstiter", "finalizer"), is_heaptype=False
)


_VersionInfo = _structseq_new_type(
    "sys.version_info",
    ("major", "minor", "micro", "releaselevel", "serial"),
    is_heaptype=False,
)


def _init(
    _executable,
    _python_path,
    _flags_data,
    _warnoptions,
    extend_python_path_with_stdlib,
):
    global executable
    executable = _executable
    global _base_executable
    _base_executable = _executable

    executable_dir = _dirname(executable)
    cfg = None
    try:
        cfg = open(_join(executable_dir, "pyvenv.cfg"), "r", encoding="utf-8")
    except IOError as e:
        try:
            cfg = open(_join(executable_dir, "..", "pyvenv.cfg"), "r", encoding="utf-8")
        except IOError as e:
            pass

    if cfg is not None:
        with cfg:
            for line in cfg:
                if line[0] == "#":
                    continue
                tokens = line.split()
                if len(tokens) >= 3 and tokens[0] == "home" and tokens[1] == "=":
                    executable_dir = tokens[2]
                    break

    _prefix = _join(executable_dir, "..")

    global prefix
    prefix = _prefix
    global base_exec_prefix
    base_exec_prefix = _prefix
    global base_prefix
    base_prefix = _prefix
    global exec_prefix
    exec_prefix = _prefix

    global path
    path = _python_path
    if extend_python_path_with_stdlib:
        stdlib_dir = _join(
            _prefix, "lib", f"{implementation.name}{_version.major}.{_version.minor}"
        )
        path.append(stdlib_dir)

    global flags
    flags = _Flags(_flags_data)

    global warnoptions
    warnoptions = _warnoptions


if _use_buffered_stdio:
    __stderr__ = open(_stderr_fd, "w", buffering=-1, closefd=False, encoding="utf-8")

    __stdin__ = open(_stdin_fd, "r", buffering=-1, closefd=False, encoding="utf-8")

    __stdout__ = open(_stdout_fd, "w", buffering=-1, closefd=False, encoding="utf-8")
else:
    __stderr__ = open(_stderr_fd, "wb", buffering=False, closefd=False)
    __stderr__ = TextIOWrapper(__stderr__, encoding="utf-8", line_buffering=False)

    __stdin__ = open(_stdin_fd, "rb", buffering=False, closefd=False)
    __stdin__ = TextIOWrapper(__stdin__, encoding="utf-8", line_buffering=False)

    __stdout__ = open(_stdout_fd, "wb", buffering=False, closefd=False)
    __stdout__ = TextIOWrapper(__stdout__, encoding="utf-8", line_buffering=False)


_base_executable = None  # will be set by _init


_framework = ""


_version = _VersionInfo(
    (
        (hexversion >> 24) & 0xFF,  # major
        (hexversion >> 16) & 0xFF,  # minor
        (hexversion >> 8) & 0xFF,  # micro
        _version_releaselevel,  # releaselevel
        hexversion & 0x0F,  # serial
    )
)


def _getframe(depth=0):
    _builtin()


abiflags = ""


# TODO(T86943617): Add `addaudithook`.


def audit(event, *args):
    pass  # TODO(T86943617): implement


base_exec_prefix = None  # will be set by _init


base_prefix = None  # will be set by _init


copyright = ""


def displayhook(value):
    if value is None:
        return
    # Set '_' to None to avoid recursion
    import builtins

    builtins._ = None
    text = repr(value)
    try:
        stdout.write(text)
    except UnicodeEncodeError:
        bytes = text.encode(stdout.encoding, "backslashreplace")
        if hasattr(stdout, "buffer"):
            stdout.buffer.write(bytes)
        else:
            text = bytes.decode(stdout.encoding, "strict")
            stdout.write(text)
    stdout.write("\n")
    builtins._ = value


__displayhook__ = displayhook


dont_write_bytecode = False


def exc_info():
    _builtin()


def excepthook(exc, value, tb):
    _builtin()


__excepthook__ = excepthook


exec_prefix = None  # will be set by _init


executable = None  # will be set by _init


def exit(code=_Unbound):
    if code is _Unbound:
        raise SystemExit()
    raise SystemExit(code)


flags = None  # will be set by _init


float_info = _FloatInfo(
    (
        1.79769313486231570814527423731704357e308,
        1024,
        308,
        2.22507385850720138309023271733240406e-308,
        -1021,
        -307,
        15,
        53,
        2.22044604925031308084726333618164062e-16,
        2,
        1,
    )
)


def getdefaultencoding():
    return "utf-8"


def getfilesystemencodeerrors():
    # TODO(T40363016): Allow arbitrary encodings and error handlings.
    return "surrogatepass"


def getfilesystemencoding():
    # TODO(T40363016): Allow arbitrary encodings instead of defaulting to utf-8.
    return "utf-8"


# TODO(T62600497): Enforce the recursion limit
def getrecursionlimit():
    _builtin()


def getsizeof(object, default=_Unbound):
    # It is possible (albeit difficult) to define a class without __sizeof__
    try:
        cls = type(object)
        size = cls.__sizeof__
    except AttributeError:
        if default is _Unbound:
            raise TypeError(f"Type {cls.__name__} doesn't define __sizeof__")
        return default
    result = size(object)
    if not _int_check(result):
        if default is _Unbound:
            raise TypeError("an integer is required")
        return default
    if result < 0:
        raise ValueError("__sizeof__() should return >= 0")
    return int(result)


def gettrace():
    return None


implementation = _SimpleNamespace(
    cache_tag=f"skybison-{_version.major}{_version.minor}", name="skybison", version=_version
)


def intern(string):
    _builtin()


def is_finalizing():
    _builtin()


meta_path = []


path = None  # will be set by _init


path_hooks = []


path_importer_cache = {}


platlibdir = "lib"


prefix = None  # will be set by _init


ps1 = "ypyp> "


ps2 = "..... "


pycache_prefix = None


# TODO(T62600497): Enforce the recursion limit
def setrecursionlimit(limit):
    _builtin()


def settrace(function):
    if function is None:
        return
    _unimplemented()


stderr = __stderr__


stdin = __stdin__


stdout = __stdout__


def unraisablehook(unraisable):
    _unimplemented()


__unraisablehook__ = unraisablehook


version_info = _version


warnoptions = None  # will be set by _init


def _program_name():
    _builtin()


def _calculate_path():
    """Returns a tuple representing (prefix, exec_prefix, module_search_path)"""
    # TODO(T61328507): Implement the path lookup algorithm. In the meantime, return
    # the compiled-in defaults that CPython returns when run out of a build directory.
    return "/usr/local", "/usr/local", ""


def get_asyncgen_hooks():
    return _AsyncGenHooks(_get_asyncgen_hooks())


def set_asyncgen_hooks(firstiter=_Unbound, finalizer=_Unbound):
    _builtin()
