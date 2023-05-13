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

#include <string>

#include "gdb/jit-reader.h"

#include "asserts.h"
#include "frame.h"
#include "thread.h"

// clang-format off
GDB_DECLARE_GPL_COMPATIBLE_READER
// clang-format on

typedef enum {
  JIT_NOACTION = 0,
  JIT_REGISTER_FN,
  JIT_UNREGISTER_FN
} JITActions;

struct Symbol {
  uword code;
  uword size;
  static const uword kMaxName = 100;
  char name[kMaxName + 1];

  uword end() const { return code + size; }

  bool contains(uword addr) const { return addr >= code && addr < end(); }

  void* operator new(size_t size) = delete;
  void* operator new[](size_t size) = delete;
};

struct SymbolTable {
  static uword sizeOf(uword num_entries) {
    return sizeof(SymbolTable) + num_entries * sizeof(Symbol);
  }

  static SymbolTable* allocate(uword num_entries) {
    SymbolTable* result =
        reinterpret_cast<SymbolTable*>(std::malloc(sizeOf(num_entries)));
    result->capacity_ = num_entries;
    result->num_entries_ = 0;
    CHECK(result != nullptr, "could not allocate SymbolTable");
    return result;
  }

  void addEntry(const char* name, uword code, uword size) {
    CHECK(num_entries_ < capacity_, "no more space in SymbolTable");
    uword length = std::strlen(name);
    CHECK(length < Symbol::kMaxName, "name too big");
    Symbol* symbol = &entries_[num_entries_++];
    // +1 copies nul in given_name.
    std::memcpy(static_cast<void*>(symbol->name), name, length + 1);
    symbol->code = code;
    symbol->size = size;
  }

  bool contains(uword addr) const {
    for (uword i = 0; i < numEntries(); i++) {
      if (at(i)->contains(addr)) {
        return true;
      }
    }
    return false;
  }

  uword size() const { return sizeOf(capacity_); }

  uword numEntries() const { return num_entries_; }

  const Symbol* at(uword idx) const { return &entries_[idx]; }

  uword capacity_;
  uword num_entries_;
  Symbol entries_[1];
};

struct JITCodeEntry {
 public:
  JITCodeEntry(SymbolTable* table)
      : symfile_addr(reinterpret_cast<char*>(table)),
        symfile_size(table->size()) {}

  void linkBefore(JITCodeEntry* entry) {
    next_entry = entry;
    if (entry != nullptr) {
      prev_entry = entry->prev_entry;
      entry->prev_entry = this;
    }
  }

  uword code() const { return reinterpret_cast<uword>(symfile_addr); }

  uword size() const { return symfile_size; }

 protected:
  struct JITCodeEntry* next_entry;
  struct JITCodeEntry* prev_entry;
  const char* symfile_addr;
  uint64_t symfile_size;
};

typedef struct {
  uint32_t version;
  // This should be JITActions, but need to be specific about the size.
  uint32_t action_flag;
  JITCodeEntry* relevant_entry;
  JITCodeEntry* first_entry;
} JITDescriptor;

PY_EXPORT JITDescriptor __jit_debug_descriptor = {1, JIT_NOACTION, nullptr,
                                                  nullptr};

PY_EXPORT NEVER_INLINE void __jit_debug_register_code(void);

extern "C" {
#define FOREACH_DWARF_REGISTER(V)                                              \
  V(RAX)                                                                       \
  V(RDX)                                                                       \
  V(RCX)                                                                       \
  V(RBX)                                                                       \
  V(RSI)                                                                       \
  V(RDI)                                                                       \
  V(RBP)                                                                       \
  V(RSP)                                                                       \
  V(R8)                                                                        \
  V(R9)                                                                        \
  V(R10)                                                                       \
  V(R11)                                                                       \
  V(R12)                                                                       \
  V(R13)                                                                       \
  V(R14)                                                                       \
  V(R15)                                                                       \
  V(RA)

enum DwarfRegister {
#define ENUM(reg) reg,
  FOREACH_DWARF_REGISTER(ENUM)
#undef ENUM
};

static const char* kRegNames[] = {
#define STR(reg) #reg,
    FOREACH_DWARF_REGISTER(STR)
#undef STR
};
}

static enum gdb_status readDebugInfo(struct gdb_reader_funcs* self,
                                     struct gdb_symbol_callbacks* cb,
                                     void* memory, long) {
  struct gdb_object* object = cb->object_open(cb);
  struct gdb_symtab* symtab = cb->symtab_open(cb, object, /*filename=*/"");
  self->priv_data = memory;
  const SymbolTable* table = reinterpret_cast<SymbolTable*>(memory);
  for (uword i = 0; i < table->numEntries(); i++) {
    const Symbol* symbol = table->at(i);
    cb->block_open(cb, symtab, /*parent=*/nullptr, symbol->code, symbol->end(),
                   symbol->name);
    fprintf(stderr, "symbol %s %p -> %p\n", symbol->name, (void*)symbol->code,
            (void*)symbol->end());
  }
  cb->symtab_close(cb, symtab);
  cb->object_close(cb, object);
  return GDB_SUCCESS;
}

static uword readRegisterUWord(struct gdb_unwind_callbacks* cb,
                               DwarfRegister reg) {
  struct gdb_reg_value* reg_value = cb->reg_get(cb, reg);
  CHECK(reg_value->defined, "register %s not defined", kRegNames[reg]);
  CHECK(reg_value->size == kWordSize, "register %s is the wrong size",
        kRegNames[reg]);
  uword result;
  std::memcpy(&result, reg_value->value, sizeof result);
  reg_value->free(reg_value);
  return result;
}

static void writeRegisterUWord(struct gdb_unwind_callbacks* cb,
                               DwarfRegister reg, uword value) {
  struct gdb_reg_value* reg_value =
      static_cast<gdb_reg_value*>(std::malloc(sizeof *reg_value + kWordSize));
  CHECK(reg_value != nullptr, "could not allocate gdb_reg_value for %s",
        kRegNames[reg]);
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

constexpr DwarfRegister kUsedCalleeSavedRegs[] = {RBX, R12, R13, R14, R15};
const word kNumCalleeSavedRegs = ARRAYSIZE(kUsedCalleeSavedRegs);
const word kFrameOffset = -kNumCalleeSavedRegs * kPointerSize;
const word kPaddingBytes = (kFrameOffset % 16) == 0 ? 0 : kPointerSize;
const word kNativeStackFrameSize = -kFrameOffset + kPaddingBytes;

static uword stackPop(struct gdb_unwind_callbacks* cb, DwarfRegister dst) {
  uword rsp = readRegisterUWord(cb, RSP);
  uword result;
  enum gdb_status read_result = cb->target_read(rsp, &result, sizeof result);
  CHECK(read_result == GDB_SUCCESS, "cannot handle read failure");
  writeRegisterUWord(cb, RSP, rsp + kPointerSize);
  writeRegisterUWord(cb, dst, result);
  return result;
}

static enum gdb_status unwindPythonFrame(struct gdb_reader_funcs* self,
                                         struct gdb_unwind_callbacks* cb) {
  CHECK(self->priv_data != nullptr, "need symbol table");
  SymbolTable* table = static_cast<SymbolTable*>(self->priv_data);
  uword ip = readRegisterUWord(cb, RA);
  if (!table->contains(ip)) {
    return GDB_FAIL;
  }
  uword frame = readRegisterUWord(cb, kFrameReg);
  if (frame == 0) {
    fprintf(stderr, "we hit bottom, boys\n");
    // We hit the end of the interpreter frame chain; try to find the C frame.
    // This is like do_return in interpreter gen
    uword rbp = readRegisterUWord(cb, RBP);
    writeRegisterUWord(cb, RSP, rbp - kNativeStackFrameSize);
    for (word i = kNumCalleeSavedRegs - 1; i >= 0; --i) {
      stackPop(cb, kUsedCalleeSavedRegs[i]);
    }
    stackPop(cb, RBP);
    uword return_address;
    uword rsp = readRegisterUWord(cb, RSP);
    MEMORY_READ(cb, rsp + kPointerSize, return_address);
    // writeRegisterUWord(cb, RA, return_address);
    // writeRegisterUWord(cb, kFrameReg, frame);
    return GDB_SUCCESS;
  }
  fprintf(stderr, "reading a python frame %p\n", (void*)frame);
  uword previous_frame;
  MEMORY_READ(cb, frame + py::Frame::kPreviousFrameOffset, previous_frame);
  writeRegisterUWord(cb, kFrameReg, previous_frame);
  // This is kind of a lie, but I am not sure how to restore the actual return
  // address inside the interpreter. Just restart at the beginning of the
  // interpreter for now.
  // TODO(emacs): Fix
  uword thread = readRegisterUWord(cb, kThreadReg);
  uword asm_interpreter;
  MEMORY_READ(cb, thread + py::Thread::interpreterFuncOffset(),
              asm_interpreter);
  fprintf(stderr, "asm interpreter in unwind: %p\n", (void*)asm_interpreter);
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
  uword ip = readRegisterUWord(cb, RA);
  uword sp = readRegisterUWord(cb, RSP);
  struct gdb_frame_id result;
  result.code_address = ip;
  result.stack_address = sp;
  return result;
}

static void destroyReader(struct gdb_reader_funcs* self) { delete self; }

struct gdb_reader_funcs* gdb_init_reader(void) {
  struct gdb_reader_funcs* result = new gdb_reader_funcs;
  result->reader_version = GDB_READER_INTERFACE_VERSION;
  result->priv_data = nullptr;
  result->read = readDebugInfo;
  result->unwind = unwindPythonFrame;
  result->get_frame_id = pythonFrameId;
  result->destroy = destroyReader;
  return result;
}

namespace py {
void gdbSupportAddFunction(const char* name, uword code, uword size) {
  DCHECK(name != nullptr, "need non-null name");
  DCHECK(code != 0, "need non-null code");
  DCHECK(size != 0, "need non-empty code");
  SymbolTable* table = SymbolTable::allocate(1);
  table->addEntry(name, code, size);
  JITCodeEntry* entry = new JITCodeEntry(table);
  entry->linkBefore(__jit_debug_descriptor.first_entry);
  __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
  __jit_debug_descriptor.first_entry = entry;
  __jit_debug_descriptor.relevant_entry = entry;
  __jit_debug_register_code();
}
}  // namespace py
