# Installing Skybison

This guide details the recommended Linux setup. A macOS (Apple Silicon) build
is also supported; see [macOS (Apple Silicon)](#macos-apple-silicon) below.

## Preparation

Install CMake and either Make or Ninja. On Fedora, this can be done with:

```sh
$ sudo dnf install cmake ninja-build
```

## Build preparation (cmake)

```sh
$ git clone https://github.com/facebookexperimental/skybison
$ cd skybison
$ mkdir build
$ cmake -B build -GNinja -DCMAKE_TOOLCHAIN_FILE=util/linux.cmake
# or, with Make
$ cmake -B build -GMake -DCMAKE_TOOLCHAIN_FILE=util/linux.cmake
```

## Build

```sh
$ ninja -C build
# or, with Make
$ make -C build
```

## Enjoy!

```sh
$ ./build/bin/python
Python 3.7.4+ (default)
[GCC Clang 9.0.20190721 ] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> print("Hello " + "world!")
Hello world!
>>> import sys
>>> sys.implementation.name
'skybison'
```

---

## macOS (Apple Silicon)

Skybison builds and runs on arm64 macOS using Homebrew for the dependency
libraries. There is no hand-written assembly interpreter or JIT on arm64, so
the runtime always uses the portable C++ interpreter (`_builtins._jit(fn)`
returns `False`).

### Preparation

Install the build tools and dependency libraries with Homebrew:

```sh
$ brew install cmake ninja \
    bzip2 libffi ncurses openssl@3 readline sqlite xz zlib
```

Module freezing (`util/freeze_modules.py`) must run under **CPython 3.8** — it
asserts the 3.8 marshal magic number. The host `python3` is almost certainly
newer, so install a 3.8 with [uv](https://docs.astral.sh/uv/):

```sh
$ uv python install 3.8
```

### Build preparation (cmake)

```sh
$ git clone https://github.com/facebookexperimental/skybison
$ cd skybison
$ cmake -S . -B build -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=util/macos.cmake \
    -DENABLE_CPYTHON_TESTS=OFF \
    -DPYTHON="$(uv python find 3.8)"
```

- `-DCMAKE_TOOLCHAIN_FILE=util/macos.cmake` points the dependency libraries at
  Homebrew's `/opt/homebrew/opt` layout.
- `-DPYTHON=...` supplies the CPython 3.8 host used to freeze modules,
  overriding the toolchain default.
- `-DENABLE_CPYTHON_TESTS=OFF` keeps the CPython-3.8-from-source build (only
  needed by the `cpython-tests` target) out of the build graph; it is not
  required to build or run the interpreter.

### Build

```sh
$ ninja -C build python
```

### Enjoy!

```sh
$ ./build/bin/python -c 'import sys; print(sys.implementation.name)'
skybison
```

---

## Build modes

You can change build type by adding the one of the following to the `cmake` command:

```
-DCMAKE_BUILD_TYPE=Release  // default
-DCMAKE_BUILD_TYPE=Debug    // -g
-DCMAKE_BUILD_TYPE=DebugOpt // -O2 -g with debug checks but faster
-DCMAKE_BUILD_TYPE=RelWithDebInfo // like a release build but has debug symbols -- useful for callgrind
```

To run tests with address sanitizer (ASAN), add

```
-DSKYBISON_ASAN=1
```

*before* the platform. Like `cmake -DSKYBISON_ASAN=1 ...`

To run tests with the undefined behavior sanitizer (UBSAN), add

```
-DSKYBISON_UBSAN=1
```

*before* the platform. Like `cmake -DSKYBISON_UBSAN=1 ...`

Since you will likely have several `build-XYZ` folders hanging around, and it's
useful for those not to show up in `git status`, they should be ignored by
default in `.gitignore`.

## Alternate linkers

The [mold](https://github.com/rui314/mold) linker is very fast and is superb
for use in development. To specify it as your linker,

1. Install it
1. Either
   * Provide it as part of the CMake command with `-DSKYBISON_LINKER=mold`, or
   * Have it preload into `ninja`/`make` with `mold -run ninja ...`
