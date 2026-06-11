# Toolchain settings for a Homebrew-based macOS build (Apple Silicon).
#
# The dependency libraries come from Homebrew under /opt/homebrew/opt. Module
# freezing (util/freeze_modules.py) requires a CPython 3.8 host interpreter
# because it asserts the 3.8 marshal magic number; install one with
# `uv python install 3.8` and pass its path via -DPYTHON=$(uv python find 3.8).

# TODO: generate a macOS-specific _sysconfigdata. The Linux one works for a
# basic build but reports Linux build flags via the sysconfig module.
set(SYSCONFIGDATA ${CMAKE_CURRENT_LIST_DIR}/linux/_sysconfigdata__linux_.py)

set(PYTHON python3.8 CACHE STRING "Python 3.8 interpreter used to freeze modules")

set(HOMEBREW_PREFIX /opt/homebrew/opt)

set(BZIP2_LIBRARIES bz2)
set(BZIP2_INCLUDE_DIRS ${HOMEBREW_PREFIX}/bzip2/include)
set(BZIP2_LIBRARY_DIRS ${HOMEBREW_PREFIX}/bzip2/lib)

set(FFI_LIBRARIES ffi)
set(FFI_INCLUDE_DIRS ${HOMEBREW_PREFIX}/libffi/include)
set(FFI_LIBRARY_DIRS ${HOMEBREW_PREFIX}/libffi/lib)

# macOS ncurses bundles terminfo; there is no separate -ltinfo.
set(NCURSES_LIBRARIES ncurses)
set(NCURSES_INCLUDE_DIRS ${HOMEBREW_PREFIX}/ncurses/include)
set(NCURSES_LIBRARY_DIRS ${HOMEBREW_PREFIX}/ncurses/lib)

set(OPENSSL_PREFIX ${HOMEBREW_PREFIX}/openssl@3)
set(OPENSSL_LIBRARIES crypto ssl)
set(OPENSSL_INCLUDE_DIRS ${HOMEBREW_PREFIX}/openssl@3/include)
set(OPENSSL_LIBRARY_DIRS ${HOMEBREW_PREFIX}/openssl@3/lib)

set(READLINE_LIBRARIES readline)
set(READLINE_INCLUDE_DIRS ${HOMEBREW_PREFIX}/readline/include)
set(READLINE_LIBRARY_DIRS ${HOMEBREW_PREFIX}/readline/lib)

set(SQLITE_LIBRARIES sqlite3)
set(SQLITE_INCLUDE_DIRS ${HOMEBREW_PREFIX}/sqlite/include)
set(SQLITE_LIBRARY_DIRS ${HOMEBREW_PREFIX}/sqlite/lib)

set(XZ_LIBRARIES lzma)
set(XZ_INCLUDE_DIRS ${HOMEBREW_PREFIX}/xz/include)
set(XZ_LIBRARY_DIRS ${HOMEBREW_PREFIX}/xz/lib)

set(ZLIB_LIBRARIES z)
set(ZLIB_INCLUDE_DIRS ${HOMEBREW_PREFIX}/zlib/include)
set(ZLIB_LIBRARY_DIRS ${HOMEBREW_PREFIX}/zlib/lib)

# The link rules reference these libraries by bare name (e.g. -lbz2). Add the
# Homebrew lib directories to the linker search path so they are found.
# PLATFORM_LINKER_FLAGS is appended to the link flags by the top-level
# CMakeLists.txt.
set(PLATFORM_LINKER_FLAGS
    "-L${BZIP2_LIBRARY_DIRS} -L${FFI_LIBRARY_DIRS} -L${NCURSES_LIBRARY_DIRS} \
-L${OPENSSL_LIBRARY_DIRS} -L${READLINE_LIBRARY_DIRS} -L${SQLITE_LIBRARY_DIRS} \
-L${XZ_LIBRARY_DIRS} -L${ZLIB_LIBRARY_DIRS}")
