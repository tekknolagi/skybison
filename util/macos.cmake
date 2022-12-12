set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(SYSCONFIGDATA ${CMAKE_CURRENT_LIST_DIR}/linux/_sysconfigdata__linux_.py)

find_package(openssl REQUIRED)

set(PLATFORM_COMPILER_FLAGS -Wno-error=deprecated-declarations)

set(XZ_INCLUDE_DIRS "/Users/emacs/.local/homebrew/opt/xz/include")
set(OPENSSL_INCLUDE_DIRS "/Users/emacs/.local/homebrew/opt/openssl/include")

set(BZIP2_LIBRARIES bz2)
set(FFI_LIBRARIES ffi)
set(NCURSES_LIBRARIES ncurses tinfo)
set(OPENSSL_PREFIX "/Users/emacs/.local/homebrew/opt")
set(OPENSSL_LIBRARIES crypto ssl)
set(READLINE_LIBRARIES readline)
set(SQLITE_LIBRARIES sqlite3)
set(XZ_LIBRARIES xz)
set(ZLIB_LIBRARIES z)
