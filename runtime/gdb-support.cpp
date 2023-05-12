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

// clang-format off
GDB_DECLARE_GPL_COMPATIBLE_READER
// clang-format on

extern "C" {
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
  RA,
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

static void writeRegisterUWord(struct gdb_unwind_callbacks* cb,
                               DwarfRegister reg, uword value) {
  struct gdb_reg_value* reg_value =
      static_cast<gdb_reg_value*>(std::malloc(sizeof *reg_value + kWordSize));
  CHECK(reg_value != nullptr, "could not allocate gdb_reg_value");
  reg_value->size = kWordSize;
  reg_value->defined = 1;
  reg_value->free = reinterpret_cast<gdb_reg_value_free*>(std::free);
  std::memcpy(reg_value->value, &value, sizeof value);
  cb->reg_set(cb, reg, reg_value);
}

const DwarfRegister kFrameReg = RBX;
const DwarfRegister kThreadReg = R12;

#define MEMORY_READ(cb, address, dst)                                          \
  do {                                                                         \
    enum gdb_status result = cb->target_read(address, &dst, sizeof dst);       \
    if (result != GDB_SUCCESS) {                                               \
      return result;                                                           \
    }                                                                          \
  } while (0)

static enum gdb_status unwindPythonFrame(struct gdb_reader_funcs*,
                                         struct gdb_unwind_callbacks* cb) {
  uword frame = readRegisterUWord(cb, kFrameReg);
  uword previous_frame;
  MEMORY_READ(cb, frame + py::Frame::kPreviousFrameOffset, previous_frame);
  writeRegisterUWord(cb, kFrameReg, previous_frame);
  // This is kind of a lie, but I am not sure how to restore the actual return
  // address inside the interpreter. Just restart at the beginning of the
  // interpreter for now.
  uword thread = readRegisterUWord(cb, kThreadReg);
  uword asm_interpreter;
  MEMORY_READ(cb, thread + py::Thread::interpreterFuncOffset(),
              asm_interpreter);
  writeRegisterUWord(cb, RA, asm_interpreter);
  // RBP is unmodified
  uword rbp = readRegisterUWord(cb, RBP);
  writeRegisterUWord(cb, RBP, rbp);
  uword locals_offset;
  MEMORY_READ(cb, frame + py::Frame::kLocalsOffsetOffset, locals_offset);
  writeRegisterUWord(
      cb, RSP,
      frame + locals_offset +
          py::Frame::kImplicitGlobalsOffsetFromLocals * kPointerSize +
          kPointerSize);
  writeRegisterUWord(cb, kThreadReg, thread);
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

static void destroyReader(struct gdb_reader_funcs*) {}

struct gdb_reader_funcs* gdb_init_reader(void) {
  static struct gdb_reader_funcs result = {
      GDB_READER_INTERFACE_VERSION,
      nullptr,
      readDebugInfo,
      unwindPythonFrame,
      pythonFrameId,
      destroyReader,
  };
  return &result;
}
}
