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
#include "gdb-support.h"

#include "gdb/jit-reader.h"

#include "asserts.h"
#include "frame.h"
#include "thread.h"

extern "C" {

int plugin_is_GPL_compatible(void) { return 0; }

enum DwarfRegister {
  RAX,
  RDX,
  RCX,
  RBX,
  RSI,
  RDI,
  RBP,
  RSP,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
};

static enum gdb_status readDebugInfo(struct gdb_reader_funcs*,
                                     struct gdb_symbol_callbacks*, void*,
                                     long) {
  UNIMPLEMENTED("gdb");
}

static uword readRegisterUWord(struct gdb_unwind_callbacks* cb,
                               DwarfRegister reg) {
  struct gdb_reg_value* reg_value = cb->reg_get(cb, reg);
  CHECK(reg_value->defined, "register not defined");
  CHECK(reg_value->size == kWordSize, "register is the wrong size");
  uword result;
  std::memcpy(&result, reg_value->value, sizeof result);
  reg_value->free(reg_value);
  return result;
}

const DwarfRegister kFrameReg = RBX;
const DwarfRegister kThreadReg = R12;

static enum gdb_status unwindPythonFrame(struct gdb_reader_funcs*,
                                         struct gdb_unwind_callbacks* cb) {
  uword frame = readRegisterUWord(cb, kFrameReg);
  uword previous_frame;
  enum gdb_status result =
      cb->target_read(frame + py::Frame::kPreviousFrameOffset, &previous_frame,
                      sizeof previous_frame);
  if (result != GDB_SUCCESS) {
    return result;
  }
  struct gdb_reg_value* updated_frame = static_cast<gdb_reg_value*>(
      std::malloc(sizeof *updated_frame + kWordSize));
  CHECK(updated_frame != nullptr, "could not allocate gdb_reg_value");
  updated_frame->size = kWordSize;
  updated_frame->defined = 1;
  updated_frame->free = reinterpret_cast<gdb_reg_value_free*>(std::free);
  std::memcpy(updated_frame + offsetof(gdb_reg_value, value), &previous_frame,
              sizeof previous_frame);
  cb->reg_set(cb, kFrameReg, updated_frame);
  return GDB_SUCCESS;
}

static struct gdb_frame_id pythonFrameId(struct gdb_reader_funcs*,
                                         struct gdb_unwind_callbacks* cb) {
  uword thread = readRegisterUWord(cb, kThreadReg);
  uword code;
  enum gdb_status read_result = cb->target_read(
      thread + py::Thread::interpreterFuncOffset(), &code, sizeof code);
  CHECK(read_result == GDB_SUCCESS, "cannot handle read failure");

  uword frame = readRegisterUWord(cb, kFrameReg);

  struct gdb_frame_id result;
  result.code_address = code;
  result.stack_address = frame;
  return result;
}

static void destroyReader(struct gdb_reader_funcs* self) { delete self; }

struct gdb_reader_funcs* gdb_init_reader(void) {
  struct gdb_reader_funcs* result = new gdb_reader_funcs();
  result->reader_version = GDB_READER_INTERFACE_VERSION;
  result->priv_data = reinterpret_cast<void*>(0xdeadbeef);
  result->read = readDebugInfo;
  result->unwind = unwindPythonFrame;
  result->get_frame_id = pythonFrameId;
  result->destroy = destroyReader;
  return result;
}
}
