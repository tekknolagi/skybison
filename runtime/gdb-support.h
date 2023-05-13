/* JIT GDB declarations for Skybison.

   Copyright (C) 2023 Max Bernstein.

   This file is part of Skybison. Skybison is under a BSD-like license;
   the GDB plugin *only* is GPL. The plugin includes just two files:

      runtime/gdb-support.h
      runtime/gdb-support.cpp

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
#pragma once

#include "globals.h"

PY_EXPORT struct gdb_reader_funcs* gdb_init_reader(void);

PY_EXPORT int plugin_is_GPL_compatible(void);

namespace py {
void gdbSupportAddFunction(const char* name, uword code, uword size);
}
