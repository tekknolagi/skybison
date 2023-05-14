// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "interpreter-gen.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "assembler-x64.h"
#include "bytecode.h"
#include "event.h"
#include "frame.h"
#include "ic.h"
#include "interpreter.h"
#include "memory-region.h"
#include "os.h"
#include "register-state.h"
#include "runtime.h"
#include "thread.h"

// This file generates an assembly version of our interpreter. The default
// implementation for all opcodes calls back to the C++ version, with
// hand-written assembly versions of perf-critical opcodes. More details are
// inline with the relevant constants and functions.

namespace py {

namespace {

#if DCHECK_IS_ON()
#define COMMENT(...) __ comment(__VA_ARGS__)
#else
#define COMMENT(...)
#endif

using namespace x64;

const word kInstructionCacheLineSize = 64;

// Abbreviated x86-64 ABI:
constexpr Register kArgRegs[] = {RDI, RSI, RDX, RCX, R8, R9};
constexpr Register kReturnRegs[] = {RAX, RDX};

// Currently unused in code, but kept around for reference:
// callee-saved registers: {RSP, RBP, RBX, R12, R13, R14, R15}

constexpr Register kCallerSavedRegs[] = {RAX, RCX, RDX, RDI, RSI,
                                         R8,  R9,  R10, R11};

constexpr Register kScratchRegs[] = {RAX, RDX, R8, R9, R10, R11};

// During normal execution, the following values are live:

// Current bytecode, a RawMutableBytes.
const Register kBCReg = RCX;

// Current PC, as an index into the bytecode.
const Register kPCReg = R14;

// Current opcode argument, as a uint32_t.
const Register kOpargReg = kArgRegs[1];

// Current Frame*.
const Register kFrameReg = RBX;

// Current Thread*.
const Register kThreadReg = R12;

// Handler base address (see below for more about the handlers).
const Register kHandlersBaseReg = R13;

// Callable objects shared for function call path.
const Register kCallableReg = RDI;

// Used for signaling the return mode when jumping to entryAsm
const Register kReturnModeReg = R15;

// The native frame/stack looks like this:
// +-------------+
// | return addr |
// | saved %rbp  | <- %rbp
// | ...         |
// | ...         | <- callee-saved registers
// | ...         |
// | padding     | <- native %rsp, when materialized for a C++ call
// +-------------+
constexpr Register kUsedCalleeSavedRegs[] = {RBX, R12, R13, R14, R15};
const word kNumCalleeSavedRegs = ARRAYSIZE(kUsedCalleeSavedRegs);
const word kFrameOffset = -kNumCalleeSavedRegs * kPointerSize;
const word kPaddingBytes = (kFrameOffset % 16) == 0 ? 0 : kPointerSize;
const word kNativeStackFrameSize = -kFrameOffset + kPaddingBytes;
const word kCallStackAlignment = 16;
static_assert(kNativeStackFrameSize % 16 == 0,
              "native frame size must be multiple of 16");

// The interpreter code itself is a prologue followed by an array of
// regularly-sized opcode handlers, spaced such that the address of a handler
// can be computed with a base address and the opcode's value. A few special
// pseudo-handlers are at negative offsets from the base address, which are used
// to handle control flow such as exceptions and returning.
//
// +----------------------+
// | prologue, setup code | <- interpreter entry point
// |----------------------+
// | UNWIND handler       | <- handlers_base - 3 * kHandlerSize
// +----------------------+
// | RETURN handler       | <- handlers_base - 2 * kHandlerSize
// +----------------------+
// | YIELD handler        | <- handlers_base - 1 * kHandlerSize
// +----------------------+
// | opcode 0 handler     | <- handlers_base + 0 * kHandlerSize
// +----------------------+
// | etc...               |
// +----------------------+
// | opcode 255 handler   | <- handlers_base + 255 * kHandlerSize
// +----------------------+
const word kHandlerSizeShift = 8;
const word kHandlerSize = 1 << kHandlerSizeShift;

const Interpreter::OpcodeHandler kCppHandlers[] = {
#define OP(name, id, handler) Interpreter::handler,
    FOREACH_BYTECODE(OP)
#undef OP
};

static const int kMaxNargs = 8;

struct EmitEnv;

class WARN_UNUSED ScratchReg : public VirtualRegister {
 public:
  ScratchReg(EmitEnv* env);
  ScratchReg(EmitEnv* env, Register reg);
  ~ScratchReg();

 private:
  EmitEnv* env_;
};

// Environment shared by all emit functions.
struct EmitEnv {
  Assembler as;
  Bytecode current_op;
  const char* current_handler;
  Label unwind_handler;

  VirtualRegister bytecode{"bytecode"};
  VirtualRegister pc{"pc"};
  VirtualRegister oparg{"oparg"};
  VirtualRegister frame{"frame"};
  VirtualRegister thread{"thread"};
  VirtualRegister handlers_base{"handlers_base"};
  VirtualRegister callable{"callable"};
  VirtualRegister return_value{"return_value"};
  VirtualRegister return_mode{"return_mode"};

  View<RegisterAssignment> handler_assignment = kNoRegisterAssignment;
  Label call_handler;
  Label opcode_handlers[kNumBytecodes];

  View<RegisterAssignment> function_entry_assignment = kNoRegisterAssignment;
  Label function_entry_with_intrinsic_handler;
  Label function_entry_with_no_intrinsic_handler;
  Label function_entry_simple_interpreted_handler[kMaxNargs];
  Label function_entry_simple_builtin[kMaxNargs];

  Label call_interpreted_slow_path;
  View<RegisterAssignment> call_interpreted_slow_path_assignment =
      kNoRegisterAssignment;

  Label call_trampoline;
  View<RegisterAssignment> call_trampoline_assignment = kNoRegisterAssignment;

  Label do_return;
  View<RegisterAssignment> do_return_assignment = kNoRegisterAssignment;

  View<RegisterAssignment> return_handler_assignment = kNoRegisterAssignment;

  RegisterState register_state;
  word handler_offset;
  word counting_handler_offset;
  bool count_opcodes;
  bool in_jit = false;
};

class JitEnv : public EmitEnv {
 public:
  JitEnv(HandleScope* scope, Thread* compiling_thread, const Function& function,
         word num_opcodes)
      : function_(scope, *function),
        thread_(compiling_thread),
        num_opcodes_(num_opcodes) {
    in_jit = true;
    opcode_handlers = new Label[num_opcodes];
  }

  ~JitEnv() {
    delete[] opcode_handlers;
    opcode_handlers = nullptr;
  }

  RawObject function() { return *function_; }

  Thread* compilingThread() { return thread_; }

  word numOpcodes() { return num_opcodes_; }

  Label* opcodeAtByteOffset(word byte_offset) {
    word opcode_index = byte_offset / kCodeUnitSize;
    DCHECK_INDEX(opcode_index, num_opcodes_);
    return &opcode_handlers[opcode_index];
  }

  word virtualPC() { return virtual_pc_; }

  void setVirtualPC(word virtual_pc) { virtual_pc_ = virtual_pc; }

  BytecodeOp currentOp() { return current_op_; }

  void setCurrentOp(BytecodeOp op) { current_op_ = op; }

  View<RegisterAssignment> jit_handler_assignment = kNoRegisterAssignment;

  View<RegisterAssignment> deopt_assignment = kNoRegisterAssignment;

  Label deopt_handler;

 private:
  Function function_;
  Thread* thread_ = nullptr;
  word num_opcodes_ = 0;
  word virtual_pc_ = 0;
  Label* opcode_handlers = nullptr;
  BytecodeOp current_op_;
};

// This macro helps instruction-emitting code stand out while staying compact.
#define __ env->as.

// RAII helper to ensure that a region of code is nop-padded to a specific size,
// with checks that it doesn't overflow the limit.
class HandlerSizer {
 public:
  HandlerSizer(EmitEnv* env, word size)
      : env_(env), size_(size), start_cursor_(env->as.codeSize()) {}

  ~HandlerSizer() {
    word padding = start_cursor_ + size_ - env_->as.codeSize();
    CHECK(padding >= 0, "Handler for %s overflowed by %" PRIdPTR " bytes",
          env_->current_handler, -padding);
    env_->as.nops(padding);
  }

 private:
  EmitEnv* env_;
  word size_;
  word start_cursor_;
};

ScratchReg::ScratchReg(EmitEnv* env) : VirtualRegister("scratch"), env_(env) {
  env_->register_state.allocate(this, kScratchRegs);
}

ScratchReg::ScratchReg(EmitEnv* env, Register reg)
    : VirtualRegister("scratch"), env_(env) {
  env_->register_state.assign(this, reg);
}

ScratchReg::~ScratchReg() {
  if (isAssigned()) {
    env_->register_state.free(this);
  }
}

// Shorthand for the Immediate corresponding to a Bool value.
Immediate boolImmediate(bool value) {
  return Immediate(Bool::fromBool(value).raw());
}

Immediate smallIntImmediate(word value) {
  return Immediate(SmallInt::fromWord(value).raw());
}

// The offset to use to access a given offset with a HeapObject, accounting for
// the tag bias.
int32_t heapObjectDisp(int32_t offset) {
  return -Object::kHeapObjectTag + offset;
}

void emitCurrentCacheIndex(EmitEnv* env, Register dst) {
  if (env->in_jit) {
    JitEnv* jenv = static_cast<JitEnv*>(env);
    __ movq(dst, Immediate(static_cast<word>(jenv->currentOp().cache)));
    return;
  }
  __ movzwq(dst, Address(env->bytecode, env->pc, TIMES_1,
                         heapObjectDisp(-kCodeUnitSize + 2)));
}

void emitNextOpcodeImpl(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ movzbl(r_scratch,
            Address(env->bytecode, env->pc, TIMES_1, heapObjectDisp(0)));
  env->register_state.assign(&env->oparg, kOpargReg);
  __ movzbl(env->oparg,
            Address(env->bytecode, env->pc, TIMES_1, heapObjectDisp(1)));
  __ addl(env->pc, Immediate(kCodeUnitSize));
  __ shll(r_scratch, Immediate(kHandlerSizeShift));
  __ addq(r_scratch, env->handlers_base);
  env->register_state.check(env->handler_assignment);
  __ jmp(r_scratch);
  // Hint to the branch predictor that the indirect jmp never falls through to
  // here.
  __ ud2();
}

// Load the next opcode, advance PC, and jump to the appropriate handler.
void emitNextOpcodeFallthrough(EmitEnv* env) {
  if (env->in_jit) {
    JitEnv* jenv = static_cast<JitEnv*>(env);
    env->register_state.check(jenv->jit_handler_assignment);
    return;
  }
  emitNextOpcodeImpl(env);
}

void emitNextOpcode(EmitEnv* env) {
  if (env->in_jit) {
    JitEnv* jenv = static_cast<JitEnv*>(env);
    env->register_state.check(jenv->jit_handler_assignment);
    __ jmp(jenv->opcodeAtByteOffset(jenv->virtualPC()), Assembler::kFarJump);
    return;
  }
  emitNextOpcodeImpl(env);
}

enum SaveRestoreFlags {
  kVMStack = 1 << 0,
  kVMFrame = 1 << 1,
  kBytecode = 1 << 2,
  kVMPC = 1 << 3,
  kHandlerBase = 1 << 4,
  kHandlerWithoutFrameChange = kVMStack | kBytecode,
  kAllState = kVMStack | kVMFrame | kBytecode | kVMPC | kHandlerBase,
  kGenericHandler = kAllState & ~kHandlerBase,
};

void emitSaveInterpreterState(EmitEnv* env, word flags) {
  if (flags & kVMFrame) {
    __ movq(Address(env->thread, Thread::currentFrameOffset()), env->frame);
  }
  if (flags & kVMStack) {
    __ movq(Address(env->thread, Thread::stackPointerOffset()), RSP);
    __ leaq(RSP, Address(RBP, -kNativeStackFrameSize));
  }
  DCHECK((flags & kBytecode) == 0, "Storing bytecode not supported");
  if (flags & kVMPC) {
    __ movq(Address(env->frame, Frame::kVirtualPCOffset), env->pc);
  }
  DCHECK((flags & kHandlerBase) == 0, "Storing handlerbase not supported");
}

void emitRestoreInterpreterState(EmitEnv* env, word flags) {
  if (flags & kVMFrame) {
    env->register_state.assign(&env->frame, kFrameReg);
    __ movq(env->frame, Address(env->thread, Thread::currentFrameOffset()));
  }
  if (flags & kVMStack) {
    __ movq(RSP, Address(env->thread, Thread::stackPointerOffset()));
  }
  if (flags & kBytecode) {
    env->register_state.assign(&env->bytecode, kBCReg);
    __ movq(env->bytecode, Address(env->frame, Frame::kBytecodeOffset));
  }
  if (flags & kVMPC) {
    env->register_state.assign(&env->pc, kPCReg);
    __ movl(env->pc, Address(env->frame, Frame::kVirtualPCOffset));
  }
  if (flags & kHandlerBase) {
    env->register_state.assign(&env->handlers_base, kHandlersBaseReg);
    __ movq(env->handlers_base,
            Address(env->thread, Thread::interpreterDataOffset()));
  }
}

SaveRestoreFlags mayChangeFramePC(Bytecode bc) {
  // These opcodes have been manually vetted to ensure that they don't change
  // the current frame or PC (or if they do, it's through something like
  // Interpreter::callMethodN(), which restores the previous frame when it's
  // finished). This lets us avoid reloading the frame after calling their C++
  // implementations.
  switch (bc) {
    case BINARY_ADD_SMALLINT:
    case BINARY_AND_SMALLINT:
    case BINARY_FLOORDIV_SMALLINT:
    case BINARY_SUB_SMALLINT:
    case BINARY_OR_SMALLINT:
    case COMPARE_EQ_SMALLINT:
    case COMPARE_LE_SMALLINT:
    case COMPARE_NE_SMALLINT:
    case COMPARE_GE_SMALLINT:
    case COMPARE_LT_SMALLINT:
    case COMPARE_GT_SMALLINT:
    case INPLACE_ADD_SMALLINT:
    case INPLACE_SUB_SMALLINT:
    case LOAD_ATTR_INSTANCE:
    case LOAD_ATTR_INSTANCE_TYPE_BOUND_METHOD:
    case LOAD_ATTR_POLYMORPHIC:
    case STORE_ATTR_INSTANCE:
    case STORE_ATTR_INSTANCE_OVERFLOW:
    case STORE_ATTR_POLYMORPHIC:
    case LOAD_METHOD_INSTANCE_FUNCTION:
    case LOAD_METHOD_POLYMORPHIC:
      return kHandlerWithoutFrameChange;
    case CALL_FUNCTION:
    case CALL_FUNCTION_ANAMORPHIC:
      return kAllState;
    default:
      return kGenericHandler;
  }
}

template <typename FPtr>
void emitCall(EmitEnv* env, FPtr function) {
  ScratchReg r_function(env);
  // TODO(T84334712) Use call with immediate instead of movq+call.
  __ movq(r_function, Immediate(reinterpret_cast<int64_t>(function)));
  __ call(r_function);
  env->register_state.clobber(kCallerSavedRegs);
}

void emitCallReg(EmitEnv* env, Register function) {
  __ call(function);
  env->register_state.clobber(kCallerSavedRegs);
}

void emitJumpToDeopt(EmitEnv* env) {
  DCHECK(env->in_jit, "deopt not supported for non-JIT assembly");
  JitEnv* jenv = static_cast<JitEnv*>(env);
  // Set the PC to what would be in a normal opcode handler. We may not have
  // set it if the JIT handler did not need the PC.
  env->register_state.assign(&env->pc, kPCReg);
  __ movq(env->pc, Immediate(jenv->virtualPC()));
  env->register_state.check(jenv->deopt_assignment);
  __ jmp(&jenv->deopt_handler, Assembler::kFarJump);
}

void emitHandleContinue(EmitEnv* env, SaveRestoreFlags flags) {
  ScratchReg r_result(env, kReturnRegs[0]);

  Label handle_flow;
  static_assert(static_cast<int>(Interpreter::Continue::NEXT) == 0,
                "NEXT must be 0");
  __ testl(r_result, r_result);
  __ jcc(NOT_ZERO, &handle_flow, Assembler::kNearJump);

  // Note that we do not restore the `kHandlerBase` for now. That saves some
  // cycles but fail to cleanly switch interpreter handlers for stackframes that
  // are already active at the time the handlers are switched.
  emitRestoreInterpreterState(env, flags);
  emitNextOpcode(env);

  // TODO(T91195773): Decide if this special-case here makes sense or if it is
  // worth duplicating the pseudo-handlers / exposing their address via some
  // API so that we can jump directly into the DEOPT one. Would rather not have
  // a separate pseudo-handler per function.
  __ bind(&handle_flow);
  if (env->in_jit) {
    Label deopt;
    __ cmpb(r_result,
            Immediate(static_cast<byte>(Interpreter::Continue::DEOPT)));
    __ jcc(EQUAL, &deopt, Assembler::kNearJump);
    // The JIT should never get here; it should always deopt beforehand.
    __ ud2();

    __ bind(&deopt);
    // TODO(T91195826): See if we can get this data statically instead of off
    // the frame object.
    emitRestoreInterpreterState(env, kGenericHandler);
    emitJumpToDeopt(env);
  } else {
    __ shll(r_result, Immediate(kHandlerSizeShift));
    __ leaq(r_result, Address(env->handlers_base, r_result, TIMES_1,
                              -Interpreter::kNumContinues * kHandlerSize));
    env->register_state.check(env->return_handler_assignment);
    __ jmp(r_result);
  }

  env->register_state.reset();
}

void emitHandleContinueIntoInterpreter(EmitEnv* env, SaveRestoreFlags flags) {
  ScratchReg r_result(env, kReturnRegs[0]);

  Label handle_flow;
  static_assert(static_cast<int>(Interpreter::Continue::NEXT) == 0,
                "NEXT must be 0");
  __ testl(r_result, r_result);
  __ jcc(NOT_ZERO, &handle_flow, Assembler::kNearJump);

  // Note that we do not restore the `kHandlerBase` for now. That saves some
  // cycles but fail to cleanly switch interpreter handlers for stackframes that
  // are already active at the time the handlers are switched.
  emitRestoreInterpreterState(env, flags);
  emitNextOpcodeImpl(env);

  __ bind(&handle_flow);
  __ shll(r_result, Immediate(kHandlerSizeShift));
  __ leaq(r_result, Address(env->handlers_base, r_result, TIMES_1,
                            -Interpreter::kNumContinues * kHandlerSize));
  env->register_state.check(env->return_handler_assignment);
  __ jmp(r_result);
  env->register_state.reset();
}

// Emit a call to the C++ implementation of the given Bytecode, saving and
// restoring appropriate interpreter state before and after the call. This code
// is emitted as a series of stubs after the main set of handlers; it's used
// from the hot path with emitJumpToGenericHandler().
void emitGenericHandler(EmitEnv* env, Bytecode bc) {
  __ movq(kArgRegs[0], env->thread);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");

  // Sync VM state to memory and restore native stack pointer.
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);

  emitCall<Interpreter::Continue (*)(Thread*, word)>(env, kCppHandlers[bc]);

  emitHandleContinue(env, mayChangeFramePC(bc));
}

Label* genericHandlerLabel(EmitEnv* env) {
  return &env->opcode_handlers[env->current_op];
}

// Jump to the generic handler for the Bytecode being currently emitted.
void emitJumpToGenericHandler(EmitEnv* env) {
  if (env->in_jit) {
    // Just generate the jump to generic handler inline. No side table.
    emitGenericHandler(env, env->current_op);
    return;
  }
  env->register_state.check(env->handler_assignment);
  __ jmp(genericHandlerLabel(env), Assembler::kFarJump);
}

// Fallback handler for all unimplemented opcodes: call out to C++.
template <Bytecode bc>
void emitHandler(EmitEnv* env) {
  emitJumpToGenericHandler(env);
}

static void emitJumpIfSmallInt(EmitEnv* env, Register object, Label* target) {
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  __ testb(object, Immediate(Object::kSmallIntTagMask));
  __ jcc(ZERO, target, Assembler::kNearJump);
}

static void emitJumpIfNotSmallInt(EmitEnv* env, Register object,
                                  Label* target) {
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  __ testb(object, Immediate(Object::kSmallIntTagMask));
  __ jcc(NOT_ZERO, target, Assembler::kNearJump);
}

static void emitJumpIfNotBothSmallInt(EmitEnv* env, Register value0,
                                      Register value1, Register scratch,
                                      Label* target) {
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  __ movq(scratch, value1);
  __ orq(scratch, value0);
  emitJumpIfNotSmallInt(env, scratch, target);
}

static void emitJumpIfImmediate(EmitEnv* env, Register obj, Label* target,
                                bool is_near_jump) {
  ScratchReg r_scratch(env);
  // Adding `(-kHeapObjectTag) & kPrimaryTagMask` will set the lowest
  // `kPrimaryTagBits` bits to zero iff the object had a `kHeapObjectTag`.
  __ leal(r_scratch,
          Address(obj, (-Object::kHeapObjectTag) & Object::kPrimaryTagMask));
  __ testl(r_scratch, Immediate(Object::kPrimaryTagMask));
  __ jcc(NOT_ZERO, target, is_near_jump);
}

// Load the LayoutId of the RawObject in r_obj into r_dst as a SmallInt.
//
// Writes to r_dst.
void emitGetLayoutId(EmitEnv* env, Register r_dst, Register r_obj) {
  Label not_heap_object;
  emitJumpIfImmediate(env, r_obj, &not_heap_object, Assembler::kNearJump);

  // It is a HeapObject.
  static_assert(RawHeader::kLayoutIdOffset + Header::kLayoutIdBits <= 32,
                "expected layout id in lower 32 bits");
  __ movl(r_dst, Address(r_obj, heapObjectDisp(RawHeapObject::kHeaderOffset)));
  __ shrl(r_dst,
          Immediate(RawHeader::kLayoutIdOffset - Object::kSmallIntTagBits));
  __ andl(r_dst, Immediate(Header::kLayoutIdMask << Object::kSmallIntTagBits));
  Label done;
  __ jmp(&done, Assembler::kNearJump);

  __ bind(&not_heap_object);
  static_assert(static_cast<int>(LayoutId::kSmallInt) == 0,
                "Expected SmallInt LayoutId to be 0");
  __ xorl(r_dst, r_dst);
  static_assert(Object::kSmallIntTagBits == 1 && Object::kSmallIntTag == 0,
                "unexpected SmallInt tag");
  emitJumpIfSmallInt(env, r_obj, &done);

  // Immediate.
  __ movl(r_dst, r_obj);
  __ andl(r_dst, Immediate(Object::kImmediateTagMask));
  static_assert(Object::kSmallIntTag == 0, "Unexpected SmallInt tag");
  __ shll(r_dst, Immediate(Object::kSmallIntTagBits));

  __ bind(&done);
}

// Assumes r_obj is a HeapObject.
void emitJumpIfNotHasLayoutId(EmitEnv* env, Register r_obj, LayoutId layout_id,
                              Label* target) {
  // It is a HeapObject.
  ScratchReg r_scratch(env);
  static_assert(RawHeader::kLayoutIdOffset + Header::kLayoutIdBits <= 32,
                "expected layout id in lower 32 bits");
  __ movl(r_scratch,
          Address(r_obj, heapObjectDisp(RawHeapObject::kHeaderOffset)));
  __ andl(r_scratch,
          Immediate(Header::kLayoutIdMask << RawHeader::kLayoutIdOffset));
  __ cmpl(r_scratch, Immediate(static_cast<word>(layout_id)
                               << RawHeader::kLayoutIdOffset));
  __ jcc(NOT_EQUAL, target, Assembler::kNearJump);
}

void emitJumpIfNotHeapObjectWithLayoutId(EmitEnv* env, Register r_obj,
                                         LayoutId layout_id, Label* target) {
  emitJumpIfImmediate(env, r_obj, target, Assembler::kNearJump);

  // It is a HeapObject.
  emitJumpIfNotHasLayoutId(env, r_obj, layout_id, target);
}

// Convert the given register from a SmallInt to an int.
void emitConvertFromSmallInt(EmitEnv* env, Register reg) {
  __ sarq(reg, Immediate(Object::kSmallIntTagBits));
}

// Look up an inline cache entry, like icLookup(). If found, the result will be
// stored in r_dst. If not found, r_dst will be unmodified and the code will
// jump to not_found. r_layout_id should contain the output of
// emitGetLayoutId(), r_caches should hold the RawTuple of caches for the
// current function, r_index should contain the opcode argument for the current
// instruction, and r_scratch is used as scratch.
//
// Writes to r_dst, r_layout_id (to turn it into a SmallInt), r_caches, and
// r_scratch.
void emitIcLookupPolymorphic(EmitEnv* env, Label* not_found, Register r_dst,
                             Register r_layout_id, Register r_caches) {
  ScratchReg r_scratch(env);
  // Load the cache index into r_scratch
  emitCurrentCacheIndex(env, r_scratch);
  // Set r_caches = r_caches + index * kPointerSize * kPointersPerEntry.
  static_assert(kPointerSize * kIcPointersPerEntry == 1 << 4,
                "Unexpected kIcPointersPerEntry");
  // Read the first value as the polymorphic cache.
  __ shll(r_scratch, Immediate(4));
  __ movq(r_caches,
          Address(r_caches, r_scratch, TIMES_1,
                  heapObjectDisp(kIcEntryValueOffset * kPointerSize)));
  __ leaq(r_caches, Address(r_caches, heapObjectDisp(0)));
  Label done;
  for (int i = 0; i < kIcPointersPerPolyCache; i += kIcPointersPerEntry) {
    bool is_last = i + kIcPointersPerEntry == kIcPointersPerPolyCache;
    __ cmpl(Address(r_caches, (i + kIcEntryKeyOffset) * kPointerSize),
            r_layout_id);
    if (is_last) {
      __ jcc(NOT_EQUAL, not_found, Assembler::kFarJump);
      __ movq(r_dst,
              Address(r_caches, (i + kIcEntryValueOffset) * kPointerSize));
    } else {
      __ cmoveq(r_dst,
                Address(r_caches, (i + kIcEntryValueOffset) * kPointerSize));
      __ jcc(EQUAL, &done, Assembler::kNearJump);
    }
  }
  __ bind(&done);
}

void emitIcLookupMonomorphic(EmitEnv* env, Label* not_found, Register r_dst,
                             Register r_layout_id, Register r_caches) {
  ScratchReg r_scratch(env);
  // Load the cache index into r_scratch
  emitCurrentCacheIndex(env, r_scratch);
  // Set r_caches = r_caches + index * kPointerSize * kPointersPerEntry.
  static_assert(kIcPointersPerEntry == 2, "Unexpected kIcPointersPerEntry");
  __ leaq(r_scratch, Address(r_scratch, TIMES_2, 0));
  __ leaq(r_caches, Address(r_caches, r_scratch, TIMES_8, heapObjectDisp(0)));
  __ cmpl(Address(r_caches, kIcEntryKeyOffset * kPointerSize), r_layout_id);
  __ jcc(NOT_EQUAL, not_found, Assembler::kNearJump);
  __ movq(r_dst, Address(r_caches, kIcEntryValueOffset * kPointerSize));
}

// Allocate and push a BoundMethod on the stack. If the heap is full and a GC
// is needed, jump to slow_path instead. r_self and r_function will be used to
// populate the BoundMethod. r_space and r_scratch are used as scratch
// registers.
//
// Writes to r_space and r_scratch.
void emitPushBoundMethod(EmitEnv* env, Label* slow_path, Register r_self,
                         Register r_function, Register r_space) {
  ScratchReg r_scratch(env);
  __ movq(r_space, Address(env->thread, Thread::runtimeOffset()));
  __ movq(r_space,
          Address(r_space, Runtime::heapOffset() + Heap::spaceOffset()));

  __ movq(r_scratch, Address(r_space, Space::fillOffset()));
  int num_attrs = BoundMethod::kSize / kPointerSize;
  __ addq(r_scratch, Immediate(Instance::allocationSize(num_attrs)));
  __ cmpq(r_scratch, Address(r_space, Space::endOffset()));
  __ jcc(GREATER, slow_path, Assembler::kFarJump);
  __ xchgq(r_scratch, Address(r_space, Space::fillOffset()));
  RawHeader header = Header::from(num_attrs, 0, LayoutId::kBoundMethod,
                                  ObjectFormat::kObjects);
  __ movq(Address(r_scratch, 0), Immediate(header.raw()));
  __ leaq(r_scratch, Address(r_scratch, -RawBoundMethod::kHeaderOffset +
                                            Object::kHeapObjectTag));
  __ movq(Address(r_scratch, heapObjectDisp(RawBoundMethod::kSelfOffset)),
          r_self);
  __ movq(Address(r_scratch, heapObjectDisp(RawBoundMethod::kFunctionOffset)),
          r_function);
  __ pushq(r_scratch);
}

// Given a RawObject in r_obj and its LayoutId (as a SmallInt) in r_layout_id,
// load its overflow RawTuple into r_dst.
//
// Writes to r_dst.
void emitLoadOverflowTuple(EmitEnv* env, Register r_dst, Register r_layout_id,
                           Register r_obj) {
  // Both uses of TIMES_4 in this function are a shortcut to multiply the value
  // of a SmallInt by kPointerSize.
  static_assert(kPointerSize >> Object::kSmallIntTagBits == 4,
                "Unexpected values of kPointerSize and/or kSmallIntTagBits");

  // TODO(bsimmers): This sequence of loads is pretty gross. See if we can make
  // the information more accessible.

  // Load thread->runtime()
  __ movq(r_dst, Address(env->thread, Thread::runtimeOffset()));
  // Load runtime->layouts_
  __ movq(r_dst, Address(r_dst, Runtime::layoutsOffset()));
  // Load layouts_[r_layout_id]
  __ movq(r_dst, Address(r_dst, r_layout_id, TIMES_4, heapObjectDisp(0)));
  // Load layout.numInObjectAttributes
  __ movq(
      r_dst,
      Address(r_dst, heapObjectDisp(RawLayout::kNumInObjectAttributesOffset)));
  __ movq(r_dst, Address(r_obj, r_dst, TIMES_4, heapObjectDisp(0)));
}

// Push/pop from/into an attribute of r_obj, given a SmallInt offset in r_offset
// (which may be negative to signal an overflow attribute). r_layout_id should
// contain the object's LayoutId as a SmallInt and is used to look up the
// overflow tuple offset if needed.
//
// Emits the "next opcode" sequence after the in-object attribute case, binding
// next at that location, and jumps to next at the end of the overflow attribute
// case.
//
// Writes to r_offset.
void emitAttrWithOffset(EmitEnv* env, void (Assembler::*asm_op)(Address),
                        Label* next, Register r_obj, Register r_offset,
                        Register r_layout_id) {
  Label is_overflow;
  emitConvertFromSmallInt(env, r_offset);
  __ testq(r_offset, r_offset);
  __ jcc(SIGN, &is_overflow, Assembler::kNearJump);
  // In-object attribute. For now, asm_op is always pushq or popq.
  (env->as.*asm_op)(Address(r_obj, r_offset, TIMES_1, heapObjectDisp(0)));
  __ bind(next);
  emitNextOpcode(env);

  __ bind(&is_overflow);
  ScratchReg r_scratch(env);
  emitLoadOverflowTuple(env, r_scratch, r_layout_id, r_obj);
  // The real tuple index is -offset - 1, which is the same as ~offset.
  __ notq(r_offset);
  (env->as.*asm_op)(Address(r_scratch, r_offset, TIMES_8, heapObjectDisp(0)));
  __ jmp(next, Assembler::kNearJump);
}

template <>
void emitHandler<NOP>(EmitEnv* env) {
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<BINARY_ADD_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // Preserve argument values in case of overflow.
  __ movq(r_result, r_left);
  __ addq(r_result, r_right);
  __ jcc(YES_OVERFLOW, &slow_path, Assembler::kNearJump);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::binaryOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<BINARY_AND_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  __ movq(r_result, r_left);
  __ andq(r_result, r_right);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::binaryOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<BINARY_SUB_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // Preserve argument values in case of overflow.
  __ movq(r_result, r_left);
  __ subq(r_result, r_right);
  __ jcc(YES_OVERFLOW, &slow_path, Assembler::kNearJump);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::binaryOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<BINARY_MUL_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // Preserve argument values in case of overflow.
  __ movq(r_result, r_left);
  emitConvertFromSmallInt(env, r_result);
  __ imulq(r_result, r_right);
  __ jcc(YES_OVERFLOW, &slow_path, Assembler::kNearJump);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::binaryOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<BINARY_OR_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // There is not __ orq instruction here because it is in the
  // emitJumpIfNotSmallInt implementation.
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::binaryOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

// Push r_list[r_index_smallint] into the stack.
static void emitPushListAt(EmitEnv* env, Register r_list,
                           Register r_index_smallint) {
  ScratchReg r_scratch(env);
  __ movq(r_scratch, Address(r_list, heapObjectDisp(List::kItemsOffset)));
  // r_index is a SmallInt, so r_key already stores the index value * 2.
  // Therefore, applying TIMES_4 will compute index * 8.
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  static_assert(Object::kSmallIntTagBits == 1, "unexpected tag for SmallInt");
  __ pushq(Address(r_scratch, r_index_smallint, TIMES_4, heapObjectDisp(0)));
}

template <>
void emitHandler<BINARY_SUBSCR_LIST>(EmitEnv* env) {
  ScratchReg r_container(env);
  ScratchReg r_key(env);
  Label slow_path;

  __ popq(r_key);
  __ popq(r_container);
  // if (container.isList() && key.isSmallInt()) {
  emitJumpIfNotHeapObjectWithLayoutId(env, r_container, LayoutId::kList,
                                      &slow_path);
  emitJumpIfNotSmallInt(env, r_key, &slow_path);

  // if (0 <= index && index < length) {
  // length >= 0 always holds. Therefore, ABOVE_EQUAL == NOT_CARRY if r_key
  // contains a negative value (sign bit == 1) or r_key >= r_list_length.
  __ cmpq(r_key, Address(r_container, heapObjectDisp(List::kNumItemsOffset)));
  __ jcc(ABOVE_EQUAL, &slow_path, Assembler::kNearJump);

  // Push list.at(index)
  emitPushListAt(env, r_container, r_key);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_container);
  __ pushq(r_key);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[1]);
  emitCall<Interpreter::Continue (*)(Thread*, word)>(
      env, Interpreter::binarySubscrUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

void emitHeaderCountOrOverflow(EmitEnv* env, Register r_dst,
                               Register r_container) {
  // Load header().count() as a SmallInt
  // r_dst = header().count()
  __ movq(r_dst,
          Address(r_container, heapObjectDisp(RawHeapObject::kHeaderOffset)));
  __ shrq(r_dst, Immediate(RawHeader::kCountOffset - Object::kSmallIntTagBits));
  __ andq(r_dst, Immediate(Header::kCountMask << Object::kSmallIntTagBits));
  // if (r_dst == kCountOverflowFlag)
  __ cmpq(r_dst, smallIntImmediate(RawHeader::kCountOverflowFlag));
  Label done;
  __ jcc(NOT_EQUAL, &done, Assembler::kNearJump);
  __ movq(r_dst, Address(r_container,
                         heapObjectDisp(RawHeapObject::kHeaderOverflowOffset)));
  __ bind(&done);
}

// Push r_tuple[r_index_smallint] into the stack.
static void emitPushTupleAt(EmitEnv* env, Register r_tuple,
                            Register r_index_smallint) {
  // r_index is a SmallInt, so r_key already stores the index value * 2.
  // Therefore, applying TIMES_4 will compute index * 8.
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  static_assert(Object::kSmallIntTagBits == 1, "unexpected tag for SmallInt");
  __ pushq(Address(r_tuple, r_index_smallint, TIMES_4, heapObjectDisp(0)));
}

template <>
void emitHandler<BINARY_SUBSCR_TUPLE>(EmitEnv* env) {
  ScratchReg r_container(env);
  ScratchReg r_key(env);
  ScratchReg r_num_items(env);
  Label slow_path;

  __ popq(r_key);
  __ popq(r_container);
  // if (container.isTuple() && key.isSmallInt()) {
  emitJumpIfNotHeapObjectWithLayoutId(env, r_container, LayoutId::kTuple,
                                      &slow_path);
  emitJumpIfNotSmallInt(env, r_key, &slow_path);
  // r_num_items = container.headerCountOrOverflow()
  emitHeaderCountOrOverflow(env, r_num_items, r_container);
  // if (r_index >= r_num_items) goto terminate;
  // if (0 <= index && index < length) {
  // length >= 0 always holds. Therefore, ABOVE_EQUAL == NOT_CARRY if r_key
  // contains a negative value (sign bit == 1) or r_key >= r_num_items.
  __ cmpq(r_key, r_num_items);
  __ jcc(ABOVE_EQUAL, &slow_path, Assembler::kNearJump);

  // Push tuple.at(index)
  emitPushTupleAt(env, r_container, r_key);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_container);
  __ pushq(r_key);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[1]);
  emitCall<Interpreter::Continue (*)(Thread*, word)>(
      env, Interpreter::binarySubscrUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

static void emitSetReturnMode(EmitEnv* env) {
  env->register_state.assign(&env->return_mode, kReturnModeReg);
  if (env->in_jit) {
    __ movq(env->return_mode, Immediate(word{Frame::ReturnMode::kJitReturn}
                                        << Frame::kReturnModeOffset));
  } else {
    __ xorl(env->return_mode, env->return_mode);
  }
}

static void emitJumpToEntryAsm(EmitEnv* env, Register r_function) {
  env->register_state.check(env->function_entry_assignment);
  __ jmp(Address(r_function, heapObjectDisp(Function::kEntryAsmOffset)));
}

// Functions called from JIT-compiled functions emulate call/ret on the C++
// stack to avoid putting random pointers on the Python stack. This emulates
// `call'.
static void emitPseudoCall(EmitEnv* env, Register r_function) {
  DCHECK(env->in_jit, "pseudo-call not supported for non-JIT assembly");
  ScratchReg r_next(env);
  Label next;

  __ subq(RBP, Immediate(kCallStackAlignment));
  __ leaq(r_next, &next);
  __ movq(Address(RBP, -kNativeStackFrameSize), r_next);
  emitJumpToEntryAsm(env, r_function);
  // `next' label address must be able to fit in a SmallInt.
  __ align(1 << Object::kSmallIntTagBits);
  __ bind(&next);
}

static void emitFunctionCall(EmitEnv* env, Register r_function) {
  emitSetReturnMode(env);
  if (env->in_jit) {
    // TODO(T91716080): Push next opcode as return address instead of
    // emitNextOpcode second jump
    emitPseudoCall(env, r_function);
    emitNextOpcode(env);
  } else {
    emitJumpToEntryAsm(env, r_function);
  }
}

template <>
void emitHandler<BINARY_SUBSCR_MONOMORPHIC>(EmitEnv* env) {
  ScratchReg r_receiver(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_key(env);
  ScratchReg r_caches(env);

  Label slow_path;
  __ popq(r_key);
  __ popq(r_receiver);
  emitGetLayoutId(env, r_layout_id, r_receiver);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  env->register_state.assign(&env->callable, kCallableReg);
  emitIcLookupMonomorphic(env, &slow_path, env->callable, r_layout_id,
                          r_caches);

  // Call __getitem__(receiver, key)
  __ pushq(env->callable);
  __ pushq(r_receiver);
  __ pushq(r_key);
  __ movq(env->oparg, Immediate(2));
  emitFunctionCall(env, env->callable);

  __ bind(&slow_path);
  __ pushq(r_receiver);
  __ pushq(r_key);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<STORE_SUBSCR_LIST>(EmitEnv* env) {
  ScratchReg r_container(env);
  ScratchReg r_key(env);
  ScratchReg r_layout_id(env);
  Label slow_path_non_list;
  Label slow_path;

  __ popq(r_key);
  __ popq(r_container);
  // if (container.isList() && key.isSmallInt()) {
  emitJumpIfNotHeapObjectWithLayoutId(env, r_container, LayoutId::kList,
                                      &slow_path_non_list);
  emitJumpIfNotSmallInt(env, r_key, &slow_path);

  // Re-use r_layout_id to store the value (right hand side).
  __ popq(r_layout_id);

  // if (0 <= index && index < length) {
  // length >= 0 always holds. Therefore, ABOVE_EQUAL == NOT_CARRY if r_key
  // contains a negative value (sign bit == 1) or r_key >= r_list_length.
  __ cmpq(r_key, Address(r_container, heapObjectDisp(List::kNumItemsOffset)));
  __ jcc(ABOVE_EQUAL, &slow_path, Assembler::kNearJump);

  // &list.at(index)
  __ movq(r_container,
          Address(r_container, heapObjectDisp(List::kItemsOffset)));
  // r_key is a SmallInt, so r_key already stores the index value * 2.
  // Therefore, applying TIMES_4 will compute index * 8.
  static_assert(Object::kSmallIntTag == 0, "unexpected tag for SmallInt");
  static_assert(Object::kSmallIntTagBits == 1, "unexpected tag for SmallInt");
  __ movq(Address(r_container, r_key, TIMES_4, heapObjectDisp(0)), r_layout_id);

  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_layout_id);
  __ bind(&slow_path_non_list);
  __ pushq(r_container);
  __ pushq(r_key);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[1]);
  emitCall<Interpreter::Continue (*)(Thread*, word)>(
      env, Interpreter::storeSubscrUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

// TODO(T59397957): Split this into two opcodes.
template <>
void emitHandler<LOAD_ATTR_INSTANCE>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;
  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupMonomorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);

  Label next;
  emitAttrWithOffset(env, &Assembler::pushq, &next, r_base, r_scratch,
                     r_layout_id);

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

static void emitJumpIfNotTypeHasFlag(EmitEnv* env, Register r_type,
                                     Type::Flag flag, Label* target) {
  ScratchReg r_flags(env);
  __ movq(r_flags, Address(r_type, heapObjectDisp(Type::kFlagsOffset)));
  __ andq(r_flags, smallIntImmediate(flag));
  __ jcc(ZERO, target, Assembler::kNearJump);
}

template <>
void emitHandler<LOAD_TYPE>(EmitEnv* env) {
  ScratchReg r_receiver(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_result(env);
  Label slow_path;
  __ popq(r_receiver);
  emitGetLayoutId(env, r_layout_id, r_receiver);
  // Load thread->runtime()
  __ movq(r_result, Address(env->thread, Thread::runtimeOffset()));
  // Load runtime->layouts_
  __ movq(r_result, Address(r_result, Runtime::layoutsOffset()));
  // Load layouts_[r_layout_id]
  __ movq(r_result, Address(r_result, r_layout_id, TIMES_4, heapObjectDisp(0)));
  // Load layout.describedType()
  __ movq(r_result,
          Address(r_result, heapObjectDisp(Layout::kDescribedTypeOffset)));
  // if (!r_result.isType()) { bail out }
  emitJumpIfNotHasLayoutId(env, r_result, LayoutId::kType, &slow_path);
  // if (!r_result.hasFlag(Type::Flag::kHasObjectDunderClass)) { bail out }
  emitJumpIfNotTypeHasFlag(env, r_result, Type::Flag::kHasObjectDunderClass,
                           &slow_path);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_receiver);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<LOAD_ATTR_INSTANCE_TYPE_BOUND_METHOD>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;
  __ popq(r_base);
  {
    ScratchReg r_layout_id(env);
    emitGetLayoutId(env, r_layout_id, r_base);
    __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
    emitIcLookupMonomorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);
  }
  emitPushBoundMethod(env, &slow_path, r_base, r_scratch, r_caches);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

// Used when transitioning from a JIT handler to an interpreter handler.
void jitEmitGenericHandlerSetup(JitEnv* env) {
  word arg = env->currentOp().arg;
  env->register_state.assign(&env->oparg, kOpargReg);
  __ movq(env->oparg, Immediate(arg));
  env->register_state.assign(&env->pc, kPCReg);
  __ movq(env->pc, Immediate(env->virtualPC()));
  env->register_state.check(env->handler_assignment);
}

template <>
void emitHandler<LOAD_ATTR_POLYMORPHIC>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;
  Label is_function;
  Label next;

  __ popq(r_base);
  {
    ScratchReg r_layout_id(env);
    emitGetLayoutId(env, r_layout_id, r_base);
    __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
    emitIcLookupPolymorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);

    emitJumpIfNotSmallInt(env, r_scratch, &is_function);
    emitAttrWithOffset(env, &Assembler::pushq, &next, r_base, r_scratch,
                       r_layout_id);
  }

  __ bind(&is_function);
  emitPushBoundMethod(env, &slow_path, r_base, r_scratch, r_caches);
  __ jmp(&next, Assembler::kNearJump);

  __ bind(&slow_path);
  __ pushq(r_base);
  // Don't deopt because this won't rewrite.
  if (env->in_jit) {
    jitEmitGenericHandlerSetup(static_cast<JitEnv*>(env));
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<LOAD_ATTR_INSTANCE_PROPERTY>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_caches(env);

  Label slow_path;
  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  env->register_state.assign(&env->callable, kCallableReg);
  emitIcLookupMonomorphic(env, &slow_path, env->callable, r_layout_id,
                          r_caches);
  // Call getter(receiver)
  __ pushq(env->callable);
  __ pushq(r_base);
  __ movq(env->oparg, Immediate(1));
  emitFunctionCall(env, env->callable);

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<LOAD_CONST>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ movq(r_scratch, Address(env->frame, Frame::kLocalsOffsetOffset));
  __ movq(r_scratch, Address(env->frame, r_scratch, TIMES_1,
                             Frame::kFunctionOffsetFromLocals * kPointerSize));
  __ movq(r_scratch,
          Address(r_scratch, heapObjectDisp(RawFunction::kCodeOffset)));
  __ movq(r_scratch,
          Address(r_scratch, heapObjectDisp(RawCode::kConstsOffset)));
  __ movq(r_scratch,
          Address(r_scratch, env->oparg, TIMES_8, heapObjectDisp(0)));
  __ pushq(r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<LOAD_DEREF>(EmitEnv* env) {
  ScratchReg r_locals_offset(env);
  ScratchReg r_n_locals(env);

  // r_n_locals = frame->code()->nlocals();
  __ movq(r_locals_offset, Address(env->frame, Frame::kLocalsOffsetOffset));
  __ movq(r_n_locals, Address(env->frame, r_locals_offset, TIMES_1,
                              Frame::kFunctionOffsetFromLocals * kPointerSize));
  __ movq(r_n_locals,
          Address(r_n_locals, heapObjectDisp(RawFunction::kCodeOffset)));
  __ movq(r_n_locals,
          Address(r_n_locals, heapObjectDisp(Code::kNlocalsOffset)));

  {
    ScratchReg r_idx(env);
    // r_idx = code.nlocals() + arg;
    static_assert(kPointerSize == 8, "kPointerSize is expected to be 8");
    static_assert(Object::kSmallIntTagBits == 1,
                  "kSmallIntTagBits is expected to be 1");
    // nlocals already shifted by 1 as a SmallInt, so nlocals << 2 makes it
    // word-aligned.
    __ shll(r_n_locals, Immediate(2));
    __ leaq(r_idx, Address(r_n_locals, env->oparg, TIMES_8, 0));

    // cell = frame->local(r_idx) == *(locals() - r_idx - 1);
    // See Frame::local.
    __ subq(r_locals_offset, r_idx);
  }
  // Object value(&scope, cell.value());
  ScratchReg r_cell_value(env);
  __ movq(r_cell_value,
          Address(env->frame, r_locals_offset, TIMES_1, -kPointerSize));
  __ movq(r_cell_value,
          Address(r_cell_value, heapObjectDisp(Cell::kValueOffset)));
  __ cmpl(r_cell_value, Immediate(Unbound::object().raw()));
  Label slow_path;
  __ jcc(EQUAL, &slow_path, Assembler::kNearJump);
  __ pushq(r_cell_value);
  emitNextOpcode(env);

  // Handle unbound cells in the generic handler.
  __ bind(&slow_path);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<LOAD_METHOD_INSTANCE_FUNCTION>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;

  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupMonomorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);

  // Only functions are cached.
  __ pushq(r_scratch);
  __ pushq(r_base);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<LOAD_METHOD_POLYMORPHIC>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;

  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupPolymorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);

  // Only functions are cached.
  __ pushq(r_scratch);
  __ pushq(r_base);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_base);
  // Don't deopt because this won't rewrite.
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<STORE_ATTR_INSTANCE>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_cache_value(env);
  ScratchReg r_caches(env);
  Label slow_path;

  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupMonomorphic(env, &slow_path, r_cache_value, r_layout_id,
                          r_caches);
  emitConvertFromSmallInt(env, r_cache_value);
  __ popq(Address(r_base, r_cache_value, TIMES_1, heapObjectDisp(0)));
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<STORE_ATTR_INSTANCE_OVERFLOW>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_cache_value(env);
  ScratchReg r_caches(env);
  Label slow_path;

  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupMonomorphic(env, &slow_path, r_cache_value, r_layout_id,
                          r_caches);
  emitConvertFromSmallInt(env, r_cache_value);

  {
    ScratchReg r_scratch(env);
    emitLoadOverflowTuple(env, r_scratch, r_layout_id, r_base);
    // The real tuple index is -offset - 1, which is the same as ~offset.
    __ notq(r_cache_value);
    __ popq(Address(r_scratch, r_cache_value, TIMES_8, heapObjectDisp(0)));
    emitNextOpcode(env);
  }

  __ bind(&slow_path);
  __ pushq(r_base);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<STORE_ATTR_POLYMORPHIC>(EmitEnv* env) {
  ScratchReg r_base(env);
  ScratchReg r_layout_id(env);
  ScratchReg r_scratch(env);
  ScratchReg r_caches(env);
  Label slow_path;

  __ popq(r_base);
  emitGetLayoutId(env, r_layout_id, r_base);
  __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
  emitIcLookupPolymorphic(env, &slow_path, r_scratch, r_layout_id, r_caches);

  Label next;
  // We only cache SmallInt values for STORE_ATTR.
  emitAttrWithOffset(env, &Assembler::popq, &next, r_base, r_scratch,
                     r_layout_id);

  __ bind(&slow_path);
  __ pushq(r_base);
  // Don't deopt because this won't rewrite.
  emitJumpToGenericHandler(env);
}

static void emitPushCallFrame(EmitEnv* env, Label* stack_overflow) {
  ScratchReg r_initial_size(env);

  {
    ScratchReg r_total_vars(env);
    __ movq(
        r_total_vars,
        Address(env->callable, heapObjectDisp(RawFunction::kTotalVarsOffset)));
    static_assert(kPointerSize == 8, "unexpected size");
    static_assert(Object::kSmallIntTag == 0 && Object::kSmallIntTagBits == 1,
                  "unexpected tag");
    // Note: SmallInt::cast(r_total_vars).value() * kPointerSize
    //    <=> r_total_vars * 4!
    __ leaq(r_initial_size, Address(r_total_vars, TIMES_4, Frame::kSize));
  }
  {
    ScratchReg r_max_size(env);
    __ movq(r_max_size,
            Address(env->callable,
                    heapObjectDisp(RawFunction::kStacksizeOrBuiltinOffset)));
    // Same reasoning as above.
    __ leaq(r_max_size, Address(r_initial_size, r_max_size, TIMES_4, 0));

    // if (sp - max_size < thread->limit_) { goto stack_overflow; }
    __ negq(r_max_size);
    __ addq(r_max_size, RSP);
    __ cmpq(r_max_size, Address(env->thread, Thread::limitOffset()));
    env->register_state.check(env->call_interpreted_slow_path_assignment);
    __ jcc(BELOW, stack_overflow, Assembler::kFarJump);
  }

  __ subq(RSP, r_initial_size);

  // Setup the new frame:
  {
    // locals_offset = initial_size + (function.totalArgs() * kPointerSize)
    // Note that the involved registers contain smallints.
    ScratchReg r_scratch(env);
    ScratchReg r_locals_offset(env);
    __ movq(r_scratch, Address(env->callable,
                               heapObjectDisp(RawFunction::kTotalArgsOffset)));
    __ leaq(r_locals_offset, Address(r_initial_size, r_scratch, TIMES_4, 0));
    // new_frame.setLocalsOffset(locals_offset)
    __ movq(Address(RSP, Frame::kLocalsOffsetOffset), r_locals_offset);
  }
  // new_frame.setBlockStackDepthReturnMode(return_mode)
  __ movq(Address(RSP, Frame::kBlockStackDepthReturnModeOffset),
          env->return_mode);
  // new_frame.setPreviousFrame(kFrameReg)
  __ movq(Address(RSP, Frame::kPreviousFrameOffset), env->frame);
  // kBCReg = callable.rewritteBytecode(); new_frame.setBytecode(kBCReg);
  env->register_state.assign(&env->bytecode, kBCReg);
  __ movq(env->bytecode,
          Address(env->callable,
                  heapObjectDisp(RawFunction::kRewrittenBytecodeOffset)));
  __ movq(Address(RSP, Frame::kBytecodeOffset), env->bytecode);
  // new_frame.setCaches(callable.caches())
  ScratchReg r_scratch(env);
  __ movq(r_scratch,
          Address(env->callable, heapObjectDisp(RawFunction::kCachesOffset)));
  __ movq(Address(RSP, Frame::kCachesOffset), r_scratch);
  // caller_frame.setVirtualPC(kPCReg); kPCReg = 0
  emitSaveInterpreterState(env, SaveRestoreFlags::kVMPC);
  env->register_state.assign(&env->pc, kPCReg);
  __ xorl(env->pc, env->pc);

  // kFrameReg = new_frame
  __ movq(env->frame, RSP);
}

void emitPrepareCallable(EmitEnv* env, Register r_layout_id,
                         Label* prepare_callable_immediate) {
  __ cmpl(r_layout_id, Immediate(static_cast<word>(LayoutId::kBoundMethod)
                                 << RawHeader::kLayoutIdOffset));
  Label slow_path;
  __ jcc(NOT_EQUAL, &slow_path, Assembler::kFarJump);

  {
    ScratchReg r_self(env);
    ScratchReg r_oparg_saved(env);
    ScratchReg r_saved_callable(env);
    ScratchReg r_saved_bc(env);
    __ movl(r_oparg_saved, env->oparg);
    __ movq(r_saved_callable, env->callable);
    __ movq(r_saved_bc, env->bytecode);

    // thread->stackInsertAt(callable_idx,
    // BoundMethod::cast(callable).function()); Use `rep movsq` to copy RCX
    // words from RSI to RDI.
    ScratchReg r_words(env, RCX);
    __ movl(r_words, env->oparg);
    ScratchReg r_src(env, RSI);
    __ movq(r_src, RSP);
    __ subq(RSP, Immediate(kPointerSize));
    ScratchReg r_dst(env, RDI);
    __ movq(r_dst, RSP);
    __ repMovsq();
    // restore and increment kOparg.
    env->register_state.assign(&env->oparg, kOpargReg);
    __ leaq(env->oparg, Address(r_oparg_saved, 1));
    // Insert bound_method.function() and bound_method.self().
    __ movq(r_self, Address(r_saved_callable,
                            heapObjectDisp(RawBoundMethod::kSelfOffset)));
    __ movq(Address(RSP, r_oparg_saved, TIMES_8, 0), r_self);
    env->register_state.assign(&env->callable, kCallableReg);
    __ movq(env->callable,
            Address(r_saved_callable,
                    heapObjectDisp(RawBoundMethod::kFunctionOffset)));
    __ movq(Address(RSP, env->oparg, TIMES_8, 0), env->callable);
  }

  emitJumpIfNotHeapObjectWithLayoutId(env, env->callable, LayoutId::kFunction,
                                      &slow_path);
  emitFunctionCall(env, env->callable);

  // res = Interpreter::prepareCallableDunderCall(thread, nargs, nargs)
  // callable = res.function
  // nargs = res.nargs
  __ bind(prepare_callable_immediate);
  __ bind(&slow_path);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  {
    ScratchReg arg0(env, kArgRegs[0]);
    __ movq(arg0, env->thread);
    CHECK(kArgRegs[1] == env->oparg, "mismatch");
    ScratchReg arg2(env, kArgRegs[2]);
    __ movq(arg2, env->oparg);
    emitCall<Interpreter::PrepareCallableResult (*)(Thread*, word, word)>(
        env, Interpreter::prepareCallableCallDunderCall);
  }
  __ cmpl(kReturnRegs[0], Immediate(Error::exception().raw()));
  __ jcc(EQUAL, &env->unwind_handler, Assembler::kFarJump);
  emitRestoreInterpreterState(env, kHandlerWithoutFrameChange);
  env->register_state.assign(&env->callable, kCallableReg);
  __ movq(env->callable, kReturnRegs[0]);
  env->register_state.assign(&env->oparg, kOpargReg);
  __ movq(env->oparg, kReturnRegs[1]);

  emitFunctionCall(env, env->callable);
}

static void emitFunctionEntrySimpleInterpretedHandler(EmitEnv* env,
                                                      word nargs) {
  CHECK(!env->in_jit,
        "should not be emitting function entrypoints in JIT mode");
  CHECK(nargs < kMaxNargs, "only support up to %ld arguments", kMaxNargs);

  // Check that we received the right number of arguments.
  __ cmpl(env->oparg, Immediate(nargs));
  env->register_state.check(env->call_interpreted_slow_path_assignment);
  __ jcc(NOT_EQUAL, &env->call_interpreted_slow_path, Assembler::kFarJump);

  emitPushCallFrame(env, /*stack_overflow=*/&env->call_interpreted_slow_path);
  emitNextOpcode(env);

  env->register_state.check(env->call_interpreted_slow_path_assignment);
  __ jmp(&env->call_interpreted_slow_path, Assembler::kFarJump);
}

// Functions called from JIT-compiled functions emulate call/ret on the C++
// stack to avoid putting random pointers on the Python stack. If returning
// back to the JIT, find the return address. This emulates `ret'.
static void emitPseudoRet(EmitEnv* env) {
  ScratchReg r_return_address(env);

  // Load the return address from the C++ stack
  __ movq(r_return_address, Address(RBP, -kNativeStackFrameSize));
  __ addq(RBP, Immediate(kCallStackAlignment));
  // Ret.
  __ jmp(r_return_address);
}

void emitCallTrampoline(EmitEnv* env) {
  ScratchReg r_scratch(env);

  // Function::cast(callable).entry()(thread, nargs);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  __ movq(r_scratch,
          Address(env->callable, heapObjectDisp(RawFunction::kEntryOffset)));
  ScratchReg r_arg0(env, kArgRegs[0]);
  __ movq(r_arg0, env->thread);
  CHECK(kArgRegs[1] == env->oparg, "register mismatch");
  static_assert(std::is_same<Function::Entry, RawObject (*)(Thread*, word)>(),
                "type mismatch");
  emitCallReg(env, r_scratch);
  ScratchReg r_result(env, kReturnRegs[0]);
  // if (result.isErrorException()) return UNWIND;
  __ cmpl(r_result, Immediate(Error::exception().raw()));
  __ jcc(EQUAL, &env->unwind_handler, Assembler::kFarJump);
  emitRestoreInterpreterState(env, kHandlerWithoutFrameChange);
  __ pushq(r_result);
  // if (return_to_jit) ret;
  Label return_to_jit;
  __ shrq(env->return_mode, Immediate(Frame::kReturnModeOffset));
  __ cmpq(env->return_mode, Immediate(Frame::ReturnMode::kJitReturn));
  __ jcc(EQUAL, &return_to_jit, Assembler::kNearJump);
  emitNextOpcode(env);

  __ bind(&return_to_jit);
  emitPseudoRet(env);
}

void emitFunctionEntryWithNoIntrinsicHandler(EmitEnv* env, Label* next_opcode) {
  CHECK(!env->in_jit,
        "should not be emitting function entrypoints in JIT mode");
  ScratchReg r_scratch(env);

  // Check whether the call is interpreted.
  __ movl(r_scratch,
          Address(env->callable, heapObjectDisp(RawFunction::kFlagsOffset)));
  __ testl(r_scratch, smallIntImmediate(Function::Flags::kInterpreted));
  env->register_state.check(env->call_trampoline_assignment);
  __ jcc(ZERO, &env->call_trampoline, Assembler::kFarJump);

  // We only support "SimpleCall" functions. This implies `kNofree` is set
  // `kwonlyargcount==0` and no varargs/varkeyargs.
  __ testl(r_scratch, smallIntImmediate(Function::Flags::kSimpleCall));
  env->register_state.check(env->call_interpreted_slow_path_assignment);
  __ jcc(ZERO, &env->call_interpreted_slow_path, Assembler::kFarJump);

  // prepareDefaultArgs.
  __ movl(r_scratch,
          Address(env->callable, heapObjectDisp(RawFunction::kArgcountOffset)));
  __ shrl(r_scratch, Immediate(SmallInt::kSmallIntTagBits));
  __ cmpl(r_scratch, env->oparg);
  env->register_state.check(env->call_interpreted_slow_path_assignment);
  __ jcc(NOT_EQUAL, &env->call_interpreted_slow_path, Assembler::kFarJump);

  emitPushCallFrame(env, &env->call_interpreted_slow_path);

  __ bind(next_opcode);
  emitNextOpcode(env);
}

static void emitCallInterpretedSlowPath(EmitEnv* env) {
  // Interpreter::callInterpreted(thread, nargs, function)
  ScratchReg r_arg2(env, kArgRegs[2]);
  __ movq(r_arg2, env->callable);
  ScratchReg r_arg0(env, kArgRegs[0]);
  __ movq(r_arg0, env->thread);
  CHECK(kArgRegs[1] == env->oparg, "reg mismatch");
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCall<Interpreter::Continue (*)(Thread*, word, RawFunction)>(
      env, Interpreter::callInterpreted);
  emitRestoreInterpreterState(env, kHandlerBase);
  emitHandleContinueIntoInterpreter(env, kGenericHandler);
}

void emitFunctionEntryWithIntrinsicHandler(EmitEnv* env) {
  CHECK(!env->in_jit,
        "should not be emitting function entrypoints in JIT mode");
  ScratchReg r_intrinsic(env);
  // if (function.intrinsic() != nullptr)
  __ movq(r_intrinsic, Address(env->callable,
                               heapObjectDisp(RawFunction::kIntrinsicOffset)));

  // if (r_intrinsic(thread)) return Continue::NEXT;
  static_assert(std::is_same<IntrinsicFunction, bool (*)(Thread*)>(),
                "type mismatch");
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  __ pushq(env->callable);
  __ pushq(env->oparg);
  ScratchReg r_arg0(env, kArgRegs[0]);
  __ movq(r_arg0, env->thread);
  emitCallReg(env, r_intrinsic);
  ScratchReg r_result(env, kReturnRegs[0]);
  env->register_state.assign(&env->oparg, kOpargReg);
  __ popq(env->oparg);
  env->register_state.assign(&env->callable, kCallableReg);
  __ popq(env->callable);
  emitRestoreInterpreterState(env, kHandlerWithoutFrameChange);
  __ testb(r_result, r_result);
  Label next_opcode;
  __ jcc(NOT_ZERO, &next_opcode, Assembler::kFarJump);

  emitFunctionEntryWithNoIntrinsicHandler(env, &next_opcode);
}

void emitFunctionEntryBuiltin(EmitEnv* env, word nargs) {
  CHECK(!env->in_jit,
        "should not be emitting function entrypoints in JIT mode");
  Label stack_overflow;
  Label unwind;

  // prepareDefaultArgs.
  __ cmpl(env->oparg, Immediate(nargs));
  __ jcc(NOT_EQUAL, &env->call_trampoline, Assembler::kFarJump);

  // Thread::pushNativeFrame()   (roughly)
  word locals_offset = Frame::kSize + nargs * kPointerSize;
  {
    // RSP -= Frame::kSize;
    // if (RSP < thread->limit_) { goto stack_overflow; }
    __ subq(RSP, Immediate(Frame::kSize));
    __ cmpq(RSP, Address(env->thread, Thread::limitOffset()));
    env->register_state.check(env->call_trampoline_assignment);
    __ jcc(BELOW, &stack_overflow, Assembler::kFarJump);

    emitSaveInterpreterState(env, kVMPC);

    // new_frame.setPreviousFrame(kFrameReg)
    __ movq(Address(RSP, Frame::kPreviousFrameOffset), env->frame);
    // new_frame.setLocalsOffset(locals_offset)
    __ movq(Address(RSP, Frame::kLocalsOffsetOffset), Immediate(locals_offset));
    __ movq(env->frame, RSP);
  }

  // r_code = Function::cast(callable).code().code().asCPtr()
  {
    ScratchReg r_code(env);
    __ movq(r_code,
            Address(env->callable,
                    heapObjectDisp(RawFunction::kStacksizeOrBuiltinOffset)));

    // function = Function::cast(callable).stacksizeOrBuiltin().asAlignedCPtr()
    // result = ((BuiltinFunction)scratch) (thread, Arguments(frame->locals()));
    emitSaveInterpreterState(env, kVMStack | kVMFrame);
    static_assert(sizeof(Arguments) == kPointerSize, "Arguments changed");
    static_assert(
        std::is_same<BuiltinFunction, RawObject (*)(Thread*, Arguments)>(),
        "type mismatch");
    ScratchReg arg0(env, kArgRegs[0]);
    __ movq(arg0, env->thread);
    ScratchReg arg1(env, kArgRegs[1]);
    __ leaq(arg1, Address(env->frame, locals_offset));
    emitCallReg(env, r_code);
  }
  ScratchReg r_result(env, kReturnRegs[0]);

  // if (return.isErrorException()) return UNWIND;
  __ cmpl(r_result, Immediate(Error::exception().raw()));
  __ jcc(EQUAL, &unwind, Assembler::kFarJump);

  // thread->popFrame()
  __ leaq(RSP, Address(env->frame,
                       locals_offset + (Frame::kFunctionOffsetFromLocals + 1) *
                                           kPointerSize));
  __ movq(env->frame, Address(env->frame, Frame::kPreviousFrameOffset));

  emitRestoreInterpreterState(env, kBytecode | kVMPC);
  // thread->stackPush(result)
  __ pushq(r_result);
  // Check return_mode == kJitReturn
  Label return_to_jit;
  __ shrq(env->return_mode, Immediate(Frame::kReturnModeOffset));
  __ cmpq(env->return_mode, Immediate(Frame::ReturnMode::kJitReturn));
  __ jcc(EQUAL, &return_to_jit, Assembler::kNearJump);
  emitNextOpcode(env);

  __ bind(&unwind);
  __ movq(env->frame, Address(env->frame, Frame::kPreviousFrameOffset));
  emitSaveInterpreterState(env, kVMFrame);
  env->register_state.check(env->return_handler_assignment);
  __ jmp(&env->unwind_handler, Assembler::kFarJump);

  __ bind(&stack_overflow);
  __ addq(RSP, Immediate(Frame::kSize));
  __ jmp(&env->call_trampoline, Assembler::kFarJump);

  // TODO(T91716258): Split LOAD_FAST into LOAD_PARAM and LOAD_FAST. This will
  // allow us to put additional metadata in the frame (such as a return
  // address) and not have to do these shenanigans.
  __ bind(&return_to_jit);
  emitPseudoRet(env);
}

void emitCallHandler(EmitEnv* env) {
  // Check callable.
  env->register_state.assign(&env->callable, kCallableReg);
  __ movq(env->callable, Address(RSP, env->oparg, TIMES_8, 0));
  // Check whether callable is a heap object.
  static_assert(Object::kHeapObjectTag == 1, "unexpected tag");
  Label prepare_callable_immediate;
  emitJumpIfImmediate(env, env->callable, &prepare_callable_immediate,
                      Assembler::kFarJump);
  // Check whether callable is a function.
  static_assert(Header::kLayoutIdMask <= kMaxInt32, "big layout id mask");
  ScratchReg r_layout_id(env);
  __ movl(r_layout_id,
          Address(env->callable, heapObjectDisp(RawHeapObject::kHeaderOffset)));
  __ andl(r_layout_id,
          Immediate(Header::kLayoutIdMask << RawHeader::kLayoutIdOffset));
  __ cmpl(r_layout_id, Immediate(static_cast<word>(LayoutId::kFunction)
                                 << RawHeader::kLayoutIdOffset));
  Label prepare_callable_generic;
  __ jcc(NOT_EQUAL, &prepare_callable_generic, Assembler::kNearJump);
  // Jump to the function's specialized entry point.
  emitFunctionCall(env, env->callable);

  __ bind(&prepare_callable_generic);
  emitPrepareCallable(env, r_layout_id, &prepare_callable_immediate);
}

template <>
void emitHandler<CALL_FUNCTION>(EmitEnv* env) {
  // The CALL_FUNCTION handler is generated out-of-line after the handler table.
  __ jmp(&env->call_handler, Assembler::kFarJump);
}

template <>
void emitHandler<CALL_FUNCTION_TYPE_NEW>(EmitEnv* env) {
  ScratchReg r_receiver(env);
  ScratchReg r_ctor(env);
  Label slow_path;

  // r_receiver = thread->stackAt(callable_idx);
  __ movq(r_receiver, Address(RSP, env->oparg, TIMES_8, 0));
  // if (!r_receiver.isType()) goto slow_path;
  emitJumpIfNotHeapObjectWithLayoutId(env, r_receiver, LayoutId::kType,
                                      &slow_path);
  {
    ScratchReg r_caches(env);
    ScratchReg r_layout_id(env);

    __ movq(r_layout_id,
            Address(r_receiver, heapObjectDisp(Type::kInstanceLayoutIdOffset)));
    __ movq(r_caches, Address(env->frame, Frame::kCachesOffset));
    emitIcLookupMonomorphic(env, &slow_path, r_ctor, r_layout_id, r_caches);
  }
  // Use `rep movsq` to copy RCX words from RSI to RDI.
  {
    ScratchReg r_saved_bc(env);
    ScratchReg r_saved_oparg(env);

    __ movq(r_saved_bc, env->bytecode);
    __ movq(r_saved_oparg, env->oparg);

    ScratchReg r_words(env, RCX);
    __ movl(r_words, env->oparg);
    ScratchReg r_src(env, RSI);
    __ movq(r_src, RSP);
    __ subq(RSP, Immediate(kPointerSize));
    ScratchReg r_dst(env, RDI);
    __ movq(r_dst, RSP);
    __ repMovsq();
    // Restore and increment kOpargReg (nargs)
    env->register_state.assign(&env->oparg, kOpargReg);
    __ leaq(env->oparg, Address(r_saved_oparg, 1));
    // Insert cached type as cls argument to cached __new__ function
    __ movq(Address(RSP, r_saved_oparg, TIMES_8, 0), r_receiver);
    // Restore bytecode
    env->register_state.assign(&env->bytecode, kBCReg);
    __ movq(env->bytecode, r_saved_bc);
    // Put the cached ctor function as the callable
    env->register_state.assign(&env->callable, kCallableReg);
    __ movq(env->callable, r_ctor);
    __ movq(Address(RSP, env->oparg, TIMES_8, 0), env->callable);
  }
  emitFunctionCall(env, env->callable);

  __ bind(&slow_path);
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<CALL_METHOD>(EmitEnv* env) {
  Label remove_value_and_call;

  // if (thread->stackPeek(arg + 1).isUnbound()) goto remove_value_and_call;
  env->register_state.assign(&env->callable, kCallableReg);
  __ movq(env->callable, Address(RSP, env->oparg, TIMES_8, kPointerSize));
  __ cmpq(env->callable, Immediate(Unbound::object().raw()));
  __ jcc(EQUAL, &remove_value_and_call, Assembler::kNearJump);

  // Increment argument count by 1 and jump into a call handler.
  __ incl(env->oparg);
  // Jump to the function's specialized entry point.
  emitFunctionCall(env, env->callable);

  // thread->removeValueAt(arg + 1)
  __ bind(&remove_value_and_call);
  ScratchReg r_saved_rdi(env);
  __ movq(r_saved_rdi, RDI);
  ScratchReg r_saved_rsi(env);
  __ movq(r_saved_rsi, RSI);
  ScratchReg r_saved_bc(env);
  __ movq(r_saved_bc, env->bytecode);
  CHECK(env->bytecode == RCX, "rcx used as arg to repmovsq");
  // Use `rep movsq` to copy RCX words from RSI to RDI.
  {
    __ std();
    ScratchReg r_num_words(env, RCX);
    __ leaq(r_num_words, Address(env->oparg, 1));
    CHECK(env->oparg == RSI, "mismatching register");
    __ leaq(RSI, Address(RSP, env->oparg, TIMES_8, 0));
    env->oparg.free();
    ScratchReg r_dst(env, RDI);
    __ leaq(r_dst, Address(RSI, kPointerSize));
    __ repMovsq();
    __ cld();
  }
  __ addq(RSP, Immediate(kPointerSize));
  __ movq(RDI, r_saved_rdi);
  __ movq(RSI, r_saved_rsi);
  env->register_state.assign(&env->bytecode, kBCReg);
  __ movq(env->bytecode, r_saved_bc);
  __ jmp(&env->call_handler, Assembler::kFarJump);
}

void jitEmitJumpForward(EmitEnv* env) {
  DCHECK(env->in_jit, "not supported for non-JIT");
  JitEnv* jenv = static_cast<JitEnv*>(env);
  __ jmp(jenv->opcodeAtByteOffset(jenv->virtualPC() +
                                  jenv->currentOp().arg * kCodeUnitScale),
         Assembler::kFarJump);
}

void emitJumpForward(EmitEnv* env, Label* next) {
  if (env->in_jit) {
    jitEmitJumpForward(env);
    return;
  }
  static_assert(kCodeUnitScale == 2, "expect to multiply arg by 2");
  __ leaq(env->pc, Address(env->pc, env->oparg, TIMES_2, 0));
  __ jmp(next, Assembler::kNearJump);
}

void emitJumpAbsolute(EmitEnv* env) {
  if (env->in_jit) {
    JitEnv* jenv = static_cast<JitEnv*>(env);
    __ jmp(jenv->opcodeAtByteOffset(jenv->currentOp().arg * kCodeUnitScale),
           Assembler::kFarJump);
    return;
  }
  static_assert(kCodeUnitScale == 2, "expect to multiply arg by 2");
  env->register_state.assign(&env->pc, kPCReg);
  __ leaq(env->pc, Address(env->oparg, TIMES_2, 0));
}

template <>
void emitHandler<FOR_ITER_TUPLE>(EmitEnv* env) {
  ScratchReg r_iter(env);
  Label next_opcode;
  Label slow_path;
  Label terminate;

  {
    ScratchReg r_index(env);
    ScratchReg r_num_items(env);
    ScratchReg r_container(env);

    __ popq(r_iter);
    emitJumpIfNotHeapObjectWithLayoutId(env, r_iter, LayoutId::kTupleIterator,
                                        &slow_path);
    __ movq(r_index,
            Address(r_iter, heapObjectDisp(TupleIterator::kIndexOffset)));
    __ movq(r_container,
            Address(r_iter, heapObjectDisp(TupleIterator::kIterableOffset)));
    // if (r_index >= r_num_items) goto terminate;
    emitHeaderCountOrOverflow(env, r_num_items, r_container);
    __ cmpq(r_index, r_num_items);
    __ jcc(GREATER_EQUAL, &terminate, Assembler::kNearJump);
    // r_index < r_num_items.
    __ pushq(r_iter);
    // Push tuple.at(index).
    emitPushTupleAt(env, r_container, r_index);
    __ addq(r_index, smallIntImmediate(1));
    __ movq(Address(r_iter, heapObjectDisp(TupleIterator::kIndexOffset)),
            r_index);
    __ bind(&next_opcode);
    emitNextOpcode(env);
  }

  __ bind(&terminate);
  emitJumpForward(env, &next_opcode);

  __ bind(&slow_path);
  __ pushq(r_iter);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitGenericHandler(env, FOR_ITER_ANAMORPHIC);
}

template <>
void emitHandler<FOR_ITER_LIST>(EmitEnv* env) {
  ScratchReg r_iter(env);
  Label next_opcode;
  Label slow_path;
  Label terminate;

  {
    ScratchReg r_index(env);
    ScratchReg r_num_items(env);
    ScratchReg r_container(env);

    __ popq(r_iter);
    emitJumpIfNotHeapObjectWithLayoutId(env, r_iter, LayoutId::kListIterator,
                                        &slow_path);
    __ movq(r_index,
            Address(r_iter, heapObjectDisp(ListIterator::kIndexOffset)));
    __ movq(r_container,
            Address(r_iter, heapObjectDisp(ListIterator::kIterableOffset)));
    // if (r_index >= r_num_items) goto terminate;
    __ movq(r_num_items,
            Address(r_container, heapObjectDisp(List::kNumItemsOffset)));
    __ cmpq(r_index, r_num_items);
    __ jcc(GREATER_EQUAL, &terminate, Assembler::kNearJump);
    // r_index < r_num_items.
    __ pushq(r_iter);
    // Push list.at(index).
    emitPushListAt(env, r_container, r_index);
    __ addq(r_index, smallIntImmediate(1));
    __ movq(Address(r_iter, heapObjectDisp(ListIterator::kIndexOffset)),
            r_index);
    __ bind(&next_opcode);
    emitNextOpcode(env);
  }

  __ bind(&terminate);
  emitJumpForward(env, &next_opcode);

  __ bind(&slow_path);
  __ pushq(r_iter);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitGenericHandler(env, FOR_ITER_ANAMORPHIC);
}

template <>
void emitHandler<FOR_ITER_RANGE>(EmitEnv* env) {
  ScratchReg r_iter(env);
  Label next_opcode;
  Label slow_path;
  Label terminate;

  {
    ScratchReg r_length(env);
    ScratchReg r_next(env);

    __ popq(r_iter);
    emitJumpIfNotHeapObjectWithLayoutId(env, r_iter, LayoutId::kRangeIterator,
                                        &slow_path);
    __ movq(r_length,
            Address(r_iter, heapObjectDisp(RangeIterator::kLengthOffset)));
    __ cmpq(r_length, smallIntImmediate(0));
    __ jcc(EQUAL, &terminate, Assembler::kNearJump);

    // if length > 0, push iter back and the current value of next.
    __ pushq(r_iter);
    __ movq(r_next,
            Address(r_iter, heapObjectDisp(RangeIterator::kNextOffset)));
    __ pushq(r_next);
    // if length > 1 decrement next.
    __ cmpq(r_length, smallIntImmediate(1));
    Label dec_length;
    __ jcc(EQUAL, &dec_length, Assembler::kNearJump);
    // iter.setNext(next + step);
    __ addq(r_next,
            Address(r_iter, heapObjectDisp(RangeIterator::kStepOffset)));
    __ movq(Address(r_iter, heapObjectDisp(RangeIterator::kNextOffset)),
            r_next);
    // iter.setLength(length - 1);
    __ bind(&dec_length);
    __ subq(r_length, smallIntImmediate(1));
    __ movq(Address(r_iter, heapObjectDisp(RangeIterator::kLengthOffset)),
            r_length);
    __ bind(&next_opcode);
    emitNextOpcode(env);
  }

  __ bind(&terminate);
  // length == 0.
  emitJumpForward(env, &next_opcode);

  __ bind(&slow_path);
  __ pushq(r_iter);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitGenericHandler(env, FOR_ITER_ANAMORPHIC);
}

template <>
void emitHandler<LOAD_BOOL>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ leaq(r_scratch, Address(env->oparg, TIMES_2, Bool::kBoolTag));
  __ pushq(r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<LOAD_FAST_REVERSE>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ movq(r_scratch, Address(env->frame, env->oparg, TIMES_8, Frame::kSize));
  __ cmpl(r_scratch, Immediate(Error::notFound().raw()));
  env->register_state.check(env->handler_assignment);
  __ jcc(EQUAL, genericHandlerLabel(env), Assembler::kFarJump);
  __ pushq(r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<LOAD_FAST_REVERSE_UNCHECKED>(EmitEnv* env) {
  __ pushq(Address(env->frame, env->oparg, TIMES_8, Frame::kSize));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<STORE_FAST_REVERSE>(EmitEnv* env) {
  __ popq(Address(env->frame, env->oparg, TIMES_8, Frame::kSize));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<DELETE_FAST_REVERSE_UNCHECKED>(EmitEnv* env) {
  __ movq(Address(env->frame, env->oparg, TIMES_8, Frame::kSize),
          Immediate(Error::notFound().raw()));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<LOAD_IMMEDIATE>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ movsbq(r_scratch, env->oparg);
  __ pushq(r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<LOAD_GLOBAL_CACHED>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ movq(r_scratch, Address(env->frame, Frame::kCachesOffset));
  __ movq(r_scratch,
          Address(r_scratch, env->oparg, TIMES_8, heapObjectDisp(0)));
  __ pushq(Address(r_scratch, heapObjectDisp(RawValueCell::kValueOffset)));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<UNARY_NOT>(EmitEnv* env) {
  Label slow_path;
  ScratchReg r_scratch(env);

  // Handle RawBools directly; fall back to C++ for other types
  __ popq(r_scratch);
  static_assert(Bool::kTagMask == 0xff, "expected full byte tag");
  __ cmpb(r_scratch, Immediate(RawObject::kBoolTag));
  // If it had kBoolTag, then negate and push.
  __ jcc(NOT_ZERO, &slow_path, Assembler::kNearJump);
  __ xorl(r_scratch,
          Immediate(RawBool::trueObj().raw() ^ RawBool::falseObj().raw()));
  __ pushq(r_scratch);
  emitNextOpcode(env);

  // Fall back to Interpreter::isTrue
  __ bind(&slow_path);
  __ pushq(r_scratch);
  emitGenericHandler(env, UNARY_NOT);
}

static void emitPopJumpIfBool(EmitEnv* env, bool jump_value) {
  ScratchReg r_scratch(env);
  Label jump;
  Label next;

  Label* true_target = jump_value ? &jump : &next;
  Label* false_target = jump_value ? &next : &jump;
  __ popq(r_scratch);

  __ cmpl(r_scratch, boolImmediate(true));
  __ jcc(EQUAL, true_target, Assembler::kNearJump);
  __ cmpl(r_scratch, boolImmediate(false));
  __ jcc(EQUAL, false_target, Assembler::kNearJump);
  __ cmpq(r_scratch, smallIntImmediate(0));
  __ jcc(EQUAL, false_target, Assembler::kNearJump);
  __ cmpb(r_scratch, Immediate(NoneType::object().raw()));
  __ jcc(EQUAL, false_target, Assembler::kNearJump);
  // Fall back to C++ for other types.
  __ pushq(r_scratch);
  if (env->in_jit) {
    emitJumpToDeopt(env);
  } else {
    emitJumpToGenericHandler(env);
  }

  __ bind(&jump);
  emitJumpAbsolute(env);
  __ bind(&next);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<POP_JUMP_IF_FALSE>(EmitEnv* env) {
  emitPopJumpIfBool(env, false);
}

template <>
void emitHandler<POP_JUMP_IF_TRUE>(EmitEnv* env) {
  emitPopJumpIfBool(env, true);
}

void emitJumpIfBoolOrPop(EmitEnv* env, bool jump_value) {
  Label next;
  Label slow_path;
  ScratchReg r_scratch(env);

  // Handle RawBools directly; fall back to C++ for other types.
  __ popq(r_scratch);
  __ cmpl(r_scratch, boolImmediate(!jump_value));
  __ jcc(EQUAL, &next, Assembler::kNearJump);
  __ cmpl(r_scratch, boolImmediate(jump_value));
  __ jcc(NOT_EQUAL, &slow_path, Assembler::kNearJump);
  __ pushq(r_scratch);
  emitJumpAbsolute(env);
  __ bind(&next);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_scratch);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  emitJumpToGenericHandler(env);
}

template <>
void emitHandler<JUMP_IF_FALSE_OR_POP>(EmitEnv* env) {
  emitJumpIfBoolOrPop(env, false);
}

template <>
void emitHandler<JUMP_IF_TRUE_OR_POP>(EmitEnv* env) {
  emitJumpIfBoolOrPop(env, true);
}

template <>
void emitHandler<JUMP_ABSOLUTE>(EmitEnv* env) {
  emitJumpAbsolute(env);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<JUMP_FORWARD>(EmitEnv* env) {
  DCHECK(!env->in_jit, "JUMP_FORWARD should have its own JIT handler");
  static_assert(kCodeUnitScale == 2, "expect to multiply arg by 2");
  __ leaq(env->pc, Address(env->pc, env->oparg, TIMES_2, 0));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<DUP_TOP>(EmitEnv* env) {
  __ pushq(Address(RSP, 0));
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<ROT_TWO>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ popq(r_scratch);
  __ pushq(Address(RSP, 0));
  __ movq(Address(RSP, 8), r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<POP_TOP>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ popq(r_scratch);
  emitNextOpcodeFallthrough(env);
}

template <>
void emitHandler<EXTENDED_ARG>(EmitEnv* env) {
  ScratchReg r_scratch(env);

  __ shll(env->oparg, Immediate(kBitsPerByte));
  __ movzbl(r_scratch,
            Address(env->bytecode, env->pc, TIMES_1, heapObjectDisp(0)));
  __ movb(env->oparg,
          Address(env->bytecode, env->pc, TIMES_1, heapObjectDisp(1)));
  __ shll(r_scratch, Immediate(kHandlerSizeShift));
  __ addl(env->pc, Immediate(kCodeUnitSize));
  __ addq(r_scratch, env->handlers_base);
  env->register_state.check(env->handler_assignment);
  __ jmp(r_scratch);
  // Hint to the branch predictor that the indirect jmp never falls through to
  // here.
  __ ud2();
}

void emitCompareIs(EmitEnv* env, bool eq_value) {
  ScratchReg r_lhs(env);
  ScratchReg r_rhs(env);
  ScratchReg r_eq_value(env);
  ScratchReg r_neq_value(env);

  __ popq(r_rhs);
  __ popq(r_lhs);
  __ movl(r_eq_value, boolImmediate(eq_value));
  __ movl(r_neq_value, boolImmediate(!eq_value));
  __ cmpq(r_rhs, r_lhs);
  __ cmovnel(r_eq_value, r_neq_value);
  __ pushq(r_eq_value);
  emitNextOpcodeFallthrough(env);
}

static void emitCompareOpSmallIntHandler(EmitEnv* env, Condition cond) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_true(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  // Use the fast path only when both arguments are SmallInt.
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  __ movq(r_true, boolImmediate(true));
  __ movq(r_result, boolImmediate(false));
  __ cmpq(r_left, r_right);
  switch (cond) {
    case EQUAL:
      __ cmoveq(r_result, r_true);
      break;
    case NOT_EQUAL:
      __ cmovneq(r_result, r_true);
      break;
    case GREATER:
      __ cmovgq(r_result, r_true);
      break;
    case GREATER_EQUAL:
      __ cmovgeq(r_result, r_true);
      break;
    case LESS:
      __ cmovlq(r_result, r_true);
      break;
    case LESS_EQUAL:
      __ cmovleq(r_result, r_true);
      break;
    default:
      UNREACHABLE("unhandled cond");
  }
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::compareOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<COMPARE_EQ_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, EQUAL);
}

template <>
void emitHandler<COMPARE_NE_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, NOT_EQUAL);
}

template <>
void emitHandler<COMPARE_GT_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, GREATER);
}

template <>
void emitHandler<COMPARE_GE_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, GREATER_EQUAL);
}

template <>
void emitHandler<COMPARE_LT_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, LESS);
}

template <>
void emitHandler<COMPARE_LE_SMALLINT>(EmitEnv* env) {
  emitCompareOpSmallIntHandler(env, LESS_EQUAL);
}

template <>
void emitHandler<COMPARE_IS>(EmitEnv* env) {
  emitCompareIs(env, true);
}

template <>
void emitHandler<COMPARE_IS_NOT>(EmitEnv* env) {
  emitCompareIs(env, false);
}

template <>
void emitHandler<INPLACE_ADD_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;

  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // Preserve argument values in case of overflow.
  __ movq(r_result, r_left);
  __ addq(r_result, r_right);
  __ jcc(YES_OVERFLOW, &slow_path, Assembler::kNearJump);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::inplaceOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<INPLACE_SUB_SMALLINT>(EmitEnv* env) {
  ScratchReg r_right(env);
  ScratchReg r_left(env);
  ScratchReg r_result(env);
  Label slow_path;
  __ popq(r_right);
  __ popq(r_left);
  emitJumpIfNotBothSmallInt(env, r_left, r_right, r_result, &slow_path);
  // Preserve argument values in case of overflow.
  __ movq(r_result, r_left);
  __ subq(r_result, r_right);
  __ jcc(YES_OVERFLOW, &slow_path, Assembler::kNearJump);
  __ pushq(r_result);
  emitNextOpcode(env);

  __ bind(&slow_path);
  __ pushq(r_left);
  __ pushq(r_right);
  if (env->in_jit) {
    emitJumpToDeopt(env);
    return;
  }
  __ movq(kArgRegs[0], env->thread);
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  emitCurrentCacheIndex(env, kArgRegs[2]);
  CHECK(env->oparg == kArgRegs[1], "oparg expect to be in rsi");
  emitCall<Interpreter::Continue (*)(Thread*, word, word)>(
      env, Interpreter::inplaceOpUpdateCache);
  emitHandleContinue(env, kGenericHandler);
}

template <>
void emitHandler<RETURN_VALUE>(EmitEnv* env) {
  Label slow_path;
  ScratchReg r_return_value(env);

  // Go to slow_path if frame->returnMode() != Frame::kNormal;
  // frame->blockStackDepth() should always be 0 here.
  __ cmpq(Address(env->frame, Frame::kBlockStackDepthReturnModeOffset),
          Immediate(0));
  __ jcc(NOT_EQUAL, &slow_path, Assembler::kNearJump);

  // Fast path: pop return value, restore caller frame, push return value.
  __ popq(r_return_value);

  {
    ScratchReg r_scratch(env);
    // RSP = frame->frameEnd()
    //     = locals() + (kFunctionOffsetFromLocals + 1) * kPointerSize)
    // (The +1 is because we have to point behind the field)
    __ movq(r_scratch, Address(env->frame, Frame::kLocalsOffsetOffset));
    __ leaq(RSP,
            Address(env->frame, r_scratch, TIMES_1,
                    (Frame::kFunctionOffsetFromLocals + 1) * kPointerSize));
    __ movq(env->frame, Address(env->frame, Frame::kPreviousFrameOffset));
  }

  emitRestoreInterpreterState(env, kBytecode | kVMPC);
  __ pushq(r_return_value);
  emitNextOpcode(env);

  __ bind(&slow_path);
  emitSaveInterpreterState(env, kVMStack | kVMFrame);
  const word handler_offset =
      -(Interpreter::kNumContinues -
        static_cast<int>(Interpreter::Continue::RETURN)) *
      kHandlerSize;
  ScratchReg r_scratch(env);
  __ leaq(r_scratch, Address(env->handlers_base, handler_offset));
  env->register_state.check(env->return_handler_assignment);
  __ jmp(r_scratch);
}

template <>
void emitHandler<POP_BLOCK>(EmitEnv* env) {
  ScratchReg r_depth(env);
  ScratchReg r_block(env);

  // frame->blockstack()->pop()
  static_assert(Frame::kBlockStackDepthMask == 0xffffffff,
                "exepcted blockstackdepth to be low 32 bits");
  __ movl(r_depth,
          Address(env->frame, Frame::kBlockStackDepthReturnModeOffset));
  __ subl(r_depth, Immediate(kPointerSize));
  __ movq(r_block,
          Address(env->frame, r_depth, TIMES_1, Frame::kBlockStackOffset));
  __ movl(Address(env->frame, Frame::kBlockStackDepthReturnModeOffset),
          r_depth);

  emitNextOpcodeFallthrough(env);
}

word emitHandlerTable(EmitEnv* env);
void emitSharedCode(EmitEnv* env);

void emitInterpreter(EmitEnv* env) {
  // Set up a frame and save callee-saved registers we'll use.
  __ pushq(RBP);
  __ movq(RBP, RSP);
  for (Register r : kUsedCalleeSavedRegs) {
    __ pushq(r);
  }

  RegisterAssignment function_entry_assignment[] = {
      {&env->pc, kPCReg},
      {&env->oparg, kOpargReg},
      {&env->frame, kFrameReg},
      {&env->thread, kThreadReg},
      {&env->handlers_base, kHandlersBaseReg},
      {&env->callable, kCallableReg},
      {&env->return_mode, kReturnModeReg},
  };
  env->function_entry_assignment = function_entry_assignment;

  RegisterAssignment handler_assignment[] = {
      {&env->bytecode, kBCReg},   {&env->pc, kPCReg},
      {&env->oparg, kOpargReg},   {&env->frame, kFrameReg},
      {&env->thread, kThreadReg}, {&env->handlers_base, kHandlersBaseReg},
  };
  env->handler_assignment = handler_assignment;

  RegisterAssignment call_interpreted_slow_path_assignment[] = {
      {&env->pc, kPCReg},       {&env->callable, kCallableReg},
      {&env->frame, kFrameReg}, {&env->thread, kThreadReg},
      {&env->oparg, kOpargReg}, {&env->handlers_base, kHandlersBaseReg},
  };
  env->call_interpreted_slow_path_assignment =
      call_interpreted_slow_path_assignment;

  RegisterAssignment call_trampoline_assignment[] = {
      {&env->pc, kPCReg},
      {&env->callable, kCallableReg},
      {&env->frame, kFrameReg},
      {&env->thread, kThreadReg},
      {&env->oparg, kOpargReg},
      {&env->handlers_base, kHandlersBaseReg},
      {&env->return_mode, kReturnModeReg},
  };
  env->call_trampoline_assignment = call_trampoline_assignment;

  RegisterAssignment return_handler_assignment[] = {
      {&env->thread, kThreadReg},
      {&env->handlers_base, kHandlersBaseReg},
  };
  env->return_handler_assignment = return_handler_assignment;

  RegisterAssignment do_return_assignment[] = {
      {&env->return_value, kReturnRegs[0]},
  };
  env->do_return_assignment = do_return_assignment;

  env->register_state.reset();
  env->register_state.assign(&env->thread, kThreadReg);
  __ movq(env->thread, kArgRegs[0]);
  env->register_state.assign(&env->frame, kFrameReg);
  __ movq(env->frame, Address(env->thread, Thread::currentFrameOffset()));

  // frame->addReturnMode(Frame::kExitRecursiveInterpreter)
  __ orl(Address(env->frame, Frame::kBlockStackDepthReturnModeOffset +
                                 (Frame::kReturnModeOffset / kBitsPerByte)),
         Immediate(static_cast<word>(Frame::kExitRecursiveInterpreter)));

  // Load VM state into registers and jump to the first opcode handler.
  emitRestoreInterpreterState(env, kAllState);
  emitNextOpcode(env);

  __ bind(&env->do_return);
  env->register_state.resetTo(do_return_assignment);
  __ leaq(RSP, Address(RBP, -kNumCalleeSavedRegs * int{kPointerSize}));
  for (word i = kNumCalleeSavedRegs - 1; i >= 0; --i) {
    __ popq(kUsedCalleeSavedRegs[i]);
  }
  __ popq(RBP);
  __ ret();

  __ align(kInstructionCacheLineSize);

  env->count_opcodes = false;
  env->handler_offset = emitHandlerTable(env);

  env->count_opcodes = true;
  env->counting_handler_offset = emitHandlerTable(env);

  emitSharedCode(env);
  env->register_state.reset();
}

template <Bytecode bc>
void jitEmitGenericHandler(JitEnv* env) {
  jitEmitGenericHandlerSetup(env);
  emitHandler<bc>(env);
}

template <Bytecode bc>
void jitEmitHandler(JitEnv* env) {
  jitEmitGenericHandler<bc>(env);
}

template <>
void jitEmitHandler<CALL_FUNCTION>(JitEnv* env) {
  env->register_state.assign(&env->callable, kCallableReg);
  __ movq(env->callable, Address(RSP, env->currentOp().arg * kWordSize));
  jitEmitGenericHandlerSetup(env);
  Label prepare_callable;
  emitJumpIfNotHeapObjectWithLayoutId(env, env->callable, LayoutId::kFunction,
                                      &prepare_callable);
  emitFunctionCall(env, env->callable);

  __ bind(&prepare_callable);
  env->register_state.assign(&env->pc, kPCReg);
  __ movq(env->pc, Immediate(env->virtualPC()));
  emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
  {
    ScratchReg arg0(env, kArgRegs[0]);
    __ movq(arg0, env->thread);
    CHECK(kArgRegs[1] == env->oparg, "mismatch");
    ScratchReg arg2(env, kArgRegs[2]);
    __ movq(arg2, Immediate(env->currentOp().arg));
    emitCall<Interpreter::PrepareCallableResult (*)(Thread*, word, word)>(
        env, Interpreter::prepareCallableCallDunderCall);
  }
  __ cmpl(kReturnRegs[0], Immediate(Error::exception().raw()));
  __ jcc(EQUAL, &env->unwind_handler, Assembler::kFarJump);
  emitRestoreInterpreterState(env, kHandlerWithoutFrameChange);
  env->register_state.assign(&env->callable, kCallableReg);
  __ movq(env->callable, kReturnRegs[0]);
  env->register_state.assign(&env->oparg, kOpargReg);
  __ movq(env->oparg, kReturnRegs[1]);

  env->register_state.check(env->call_trampoline_assignment);
  emitCallTrampoline(env);
}

void emitPushImmediate(EmitEnv* env, word value) {
  if (Utils::fits<int32_t>(value)) {
    __ pushq(Immediate(value));
  } else {
    ScratchReg r_scratch(env);

    __ movq(r_scratch, Immediate(value));
    __ pushq(r_scratch);
  }
}

template <>
void jitEmitHandler<LOAD_BOOL>(JitEnv* env) {
  word arg = env->currentOp().arg;
  DCHECK(arg == 0x80 || arg == 0, "unexpected arg");
  word value = Bool::fromBool(arg).raw();
  __ pushq(Immediate(value));
}

template <>
void jitEmitHandler<LOAD_CONST>(JitEnv* env) {
  HandleScope scope(env->compilingThread());
  Code code(&scope, Function::cast(env->function()).code());
  Tuple consts(&scope, code.consts());
  word arg = env->currentOp().arg;
  Object value(&scope, consts.at(arg));
  if (!value.isHeapObject()) {
    emitPushImmediate(env, value.raw());
    return;
  }
  // Fall back to runtime LOAD_CONST for non-immediates like tuples, etc.
  jitEmitGenericHandler<LOAD_CONST>(env);
}

template <>
void jitEmitHandler<LOAD_IMMEDIATE>(JitEnv* env) {
  word arg = env->currentOp().arg;
  emitPushImmediate(env, objectFromOparg(arg).raw());
}

template <>
void jitEmitHandler<LOAD_FAST_REVERSE>(JitEnv* env) {
  Label slow_path;
  ScratchReg r_scratch(env);

  word arg = env->currentOp().arg;
  word frame_offset = arg * kWordSize + Frame::kSize;
  __ movq(r_scratch, Address(env->frame, frame_offset));
  __ cmpl(r_scratch, Immediate(Error::notFound().raw()));
  __ jcc(EQUAL, &slow_path, Assembler::kNearJump);
  __ pushq(r_scratch);
  emitNextOpcode(env);

  __ bind(&slow_path);
  // TODO(T90560373): Instead of deoptimizing, raise UnboundLocalError.
  jitEmitGenericHandlerSetup(env);
  emitJumpToDeopt(env);
}

template <>
void jitEmitHandler<LOAD_FAST_REVERSE_UNCHECKED>(JitEnv* env) {
  word arg = env->currentOp().arg;
  word frame_offset = arg * kWordSize + Frame::kSize;
  __ pushq(Address(env->frame, frame_offset));
}

template <>
void jitEmitHandler<JUMP_FORWARD>(JitEnv* env) {
  jitEmitJumpForward(env);
}

template <>
void jitEmitHandler<RETURN_VALUE>(JitEnv* env) {
  ScratchReg r_return_value(env);

  // The return mode for simple interpreted functions is normally 0 (see
  // emitPushCallFrame/Thread::pushCallFrameImpl), so we can skip the slow
  // path.
  // TODO(T89514778): When profiling is enabled, discard all JITed functions
  // and stop JITing.

  // Fast path: pop return value, restore caller frame, push return value.
  __ popq(r_return_value);

  {
    ScratchReg r_scratch(env);
    // RSP = frame->frameEnd()
    //     = locals() + (kFunctionOffsetFromLocals + 1) * kPointerSize)
    // (The +1 is because we have to point behind the field)
    __ movq(r_scratch, Address(env->frame, Frame::kLocalsOffsetOffset));
    __ leaq(RSP,
            Address(env->frame, r_scratch, TIMES_1,
                    (Frame::kFunctionOffsetFromLocals + 1) * kPointerSize));
    __ movq(env->frame, Address(env->frame, Frame::kPreviousFrameOffset));
  }

  // Need to restore handler base from the calling frame, which (so far) is
  // always the assembly interpreter. This allows emitNextOpcode to find a
  // handler for the next opcode.
  emitRestoreInterpreterState(env, kBytecode | kVMPC | kHandlerBase);
  __ pushq(r_return_value);
  emitNextOpcodeImpl(env);
}

bool isSupportedInJIT(Bytecode bc) {
  switch (bc) {
    case BINARY_ADD:
    case BINARY_ADD_SMALLINT:
    case BINARY_AND:
    case BINARY_AND_SMALLINT:
    case BINARY_FLOOR_DIVIDE:
    case BINARY_LSHIFT:
    case BINARY_MATRIX_MULTIPLY:
    case BINARY_MODULO:
    case BINARY_MULTIPLY:
    case BINARY_MUL_SMALLINT:
    case BINARY_OP_MONOMORPHIC:
    case BINARY_OR:
    case BINARY_OR_SMALLINT:
    case BINARY_POWER:
    case BINARY_RSHIFT:
    case BINARY_SUBSCR:
    case BINARY_SUBSCR_LIST:
    case BINARY_SUBSCR_MONOMORPHIC:
    case BINARY_SUBTRACT:
    case BINARY_SUB_SMALLINT:
    case BINARY_TRUE_DIVIDE:
    case BINARY_XOR:
    case BUILD_CONST_KEY_MAP:
    case BUILD_LIST:
    case BUILD_LIST_UNPACK:
    case BUILD_MAP:
    case BUILD_MAP_UNPACK:
    case BUILD_MAP_UNPACK_WITH_CALL:
    case BUILD_SET:
    case BUILD_SET_UNPACK:
    case BUILD_SLICE:
    case BUILD_STRING:
    case BUILD_TUPLE:
    case BUILD_TUPLE_UNPACK:
    case BUILD_TUPLE_UNPACK_WITH_CALL:
    case CALL_FUNCTION:
    case COMPARE_EQ_SMALLINT:
    case COMPARE_GE_SMALLINT:
    case COMPARE_GT_SMALLINT:
    case COMPARE_IS:
    case COMPARE_IS_NOT:
    case COMPARE_LE_SMALLINT:
    case COMPARE_LT_SMALLINT:
    case COMPARE_NE_SMALLINT:
    case COMPARE_OP:
    case DELETE_ATTR:
    case DELETE_FAST:
    case DELETE_FAST_REVERSE_UNCHECKED:
    case DELETE_NAME:
    case DELETE_SUBSCR:
    case DUP_TOP:
    case DUP_TOP_TWO:
    case FORMAT_VALUE:
    case FOR_ITER:
    case FOR_ITER_LIST:
    case FOR_ITER_RANGE:
    case GET_ANEXT:
    case GET_ITER:
    case GET_YIELD_FROM_ITER:
    case IMPORT_FROM:
    case IMPORT_STAR:
    case INPLACE_ADD:
    case INPLACE_ADD_SMALLINT:
    case INPLACE_AND:
    case INPLACE_FLOOR_DIVIDE:
    case INPLACE_LSHIFT:
    case INPLACE_MATRIX_MULTIPLY:
    case INPLACE_MODULO:
    case INPLACE_MULTIPLY:
    case INPLACE_OR:
    case INPLACE_POWER:
    case INPLACE_RSHIFT:
    case INPLACE_SUBTRACT:
    case INPLACE_SUB_SMALLINT:
    case INPLACE_TRUE_DIVIDE:
    case INPLACE_XOR:
    case JUMP_ABSOLUTE:
    case JUMP_FORWARD:
    case JUMP_IF_FALSE_OR_POP:
    case JUMP_IF_TRUE_OR_POP:
    case LIST_APPEND:
    case LOAD_ATTR:
    case LOAD_ATTR_INSTANCE:
    case LOAD_ATTR_INSTANCE_TYPE_BOUND_METHOD:
    case LOAD_ATTR_POLYMORPHIC:
    case LOAD_BOOL:
    case LOAD_BUILD_CLASS:
    case LOAD_CONST:
    case LOAD_FAST:
    case LOAD_FAST_REVERSE:
    case LOAD_FAST_REVERSE_UNCHECKED:
    case LOAD_GLOBAL_CACHED:
    case LOAD_IMMEDIATE:
    case LOAD_METHOD:
    case LOAD_NAME:
    case MAKE_FUNCTION:
    case MAP_ADD:
    case NOP:
    case POP_JUMP_IF_FALSE:
    case POP_JUMP_IF_TRUE:
    case POP_TOP:
    case PRINT_EXPR:
    case RETURN_VALUE:
    case ROT_FOUR:
    case ROT_THREE:
    case ROT_TWO:
    case SETUP_ANNOTATIONS:
    case SETUP_ASYNC_WITH:
    case SETUP_WITH:
    case SET_ADD:
    case STORE_ATTR:
    case STORE_ATTR_INSTANCE:
    case STORE_ATTR_INSTANCE_OVERFLOW:
    case STORE_ATTR_INSTANCE_UPDATE:
    case STORE_ATTR_POLYMORPHIC:
    case STORE_FAST:
    case STORE_FAST_REVERSE:
    case STORE_NAME:
    case STORE_SUBSCR:
    case STORE_SUBSCR_LIST:
    case UNARY_INVERT:
    case UNARY_NEGATIVE:
    case UNARY_NOT:
    case UNARY_POSITIVE:
    case UNPACK_EX:
    case UNPACK_SEQUENCE:
      return true;
    default:
      return false;
  }
}

void emitBeforeHandler(EmitEnv* env) {
  if (env->count_opcodes) {
    __ incq(Address(env->thread, Thread::opcodeCountOffset()));
  }
}

word emitHandlerTable(EmitEnv* env) {
  // UNWIND pseudo-handler
  static_assert(static_cast<int>(Interpreter::Continue::UNWIND) == 1,
                "Unexpected UNWIND value");
  {
    env->current_handler = "UNWIND pseudo-handler";
    env->register_state.resetTo(env->return_handler_assignment);
    HandlerSizer sizer(env, kHandlerSize);
    if (!env->unwind_handler.isBound()) {
      __ bind(&env->unwind_handler);
    }
    __ movq(kArgRegs[0], env->thread);

    // TODO(T91716258): Add JIT support here for isErrorNotFound and
    // appropriate jmp.
    emitCall<RawObject (*)(Thread*)>(env, Interpreter::unwind);
    ScratchReg r_result(env, kReturnRegs[0]);
    // Check result.isErrorError()
    __ cmpl(r_result, Immediate(Error::error().raw()));
    env->register_state.assign(&env->return_value, r_result);
    env->register_state.check(env->do_return_assignment);
    __ jcc(NOT_EQUAL, &env->do_return, Assembler::kFarJump);
    emitRestoreInterpreterState(env, kGenericHandler);
    emitNextOpcode(env);
  }

  // RETURN pseudo-handler
  static_assert(static_cast<int>(Interpreter::Continue::RETURN) == 2,
                "Unexpected RETURN value");
  {
    env->current_handler = "RETURN pseudo-handler";
    env->register_state.resetTo(env->return_handler_assignment);
    HandlerSizer sizer(env, kHandlerSize);
    __ movq(kArgRegs[0], env->thread);
    emitCall<RawObject (*)(Thread*)>(env, Interpreter::handleReturn);
    Label return_to_jit;
    {
      ScratchReg r_result(env, kReturnRegs[0]);
      // Check result.isErrorNotFound()
      __ cmpl(r_result, Immediate(Error::notFound().raw()));
      __ jcc(EQUAL, &return_to_jit, Assembler::kNearJump);
      // Check result.isErrorError()
      __ cmpl(r_result, Immediate(Error::error().raw()));
      env->register_state.assign(&env->return_value, r_result);
    }
    env->register_state.check(env->do_return_assignment);
    __ jcc(NOT_EQUAL, &env->do_return, Assembler::kFarJump);
    emitRestoreInterpreterState(env, kGenericHandler);
    emitNextOpcode(env);

    // TODO(T91716258): Split LOAD_FAST into LOAD_PARAM and LOAD_FAST. This
    // will allow us to put additional metadata in the frame (such as a return
    // address) and not have to do these shenanigans.
    __ bind(&return_to_jit);
    emitRestoreInterpreterState(env, kGenericHandler);
    emitPseudoRet(env);
  }

  // YIELD pseudo-handler
  static_assert(static_cast<int>(Interpreter::Continue::YIELD) == 3,
                "Unexpected YIELD value");
  {
    env->current_handler = "YIELD pseudo-handler";
    env->register_state.resetTo(env->return_handler_assignment);
    HandlerSizer sizer(env, kHandlerSize);
    // result = thread->stackPop()
    ScratchReg r_scratch_top(env, RDX);
    __ movq(r_scratch_top, Address(env->thread, Thread::stackPointerOffset()));
    env->register_state.assign(&env->return_value, kReturnRegs[0]);
    __ movq(env->return_value, Address(r_scratch_top, 0));
    __ addq(r_scratch_top, Immediate(kPointerSize));
    __ movq(Address(env->thread, Thread::stackPointerOffset()), r_scratch_top);

    env->register_state.check(env->do_return_assignment);
    __ jmp(&env->do_return, Assembler::kFarJump);
  }

  // DEOPT pseudo-handler
  static_assert(static_cast<int>(Interpreter::Continue::DEOPT) == 4,
                "Unexpected DEOPT value");
  {
    env->current_handler = "DEOPT pseudo-handler";
    env->register_state.resetTo(env->return_handler_assignment);
    HandlerSizer sizer(env, kHandlerSize);
    DCHECK(!env->in_jit, "DEOPT handler should not get hit");
    __ breakpoint();
    __ ud2();
  }

  word offset_0 = env->as.codeSize();

#define BC(name, i, handler)                                                   \
  {                                                                            \
    env->current_op = name;                                                    \
    env->current_handler = #name;                                              \
    HandlerSizer sizer(env, kHandlerSize);                                     \
    env->register_state.resetTo(env->handler_assignment);                      \
    emitBeforeHandler(env);                                                    \
    emitHandler<name>(env);                                                    \
  }
  FOREACH_BYTECODE(BC)
#undef BC

  env->register_state.reset();
  return offset_0;
}

void emitSharedCode(EmitEnv* env) {
  {
    // This register is shared between the following three functions.
    __ bind(&env->call_handler);
    env->register_state.resetTo(env->handler_assignment);
    emitCallHandler(env);

    __ align(16);
    __ bind(&env->function_entry_with_intrinsic_handler);
    env->register_state.resetTo(env->function_entry_assignment);
    emitFunctionEntryWithIntrinsicHandler(env);

    __ align(16);
    __ bind(&env->function_entry_with_no_intrinsic_handler);
    env->register_state.resetTo(env->function_entry_assignment);
    Label next_opcode;
    emitFunctionEntryWithNoIntrinsicHandler(env, &next_opcode);

    for (word i = 0; i < kMaxNargs; i++) {
      __ align(16);
      __ bind(&env->function_entry_simple_interpreted_handler[i]);
      env->register_state.resetTo(env->function_entry_assignment);
      emitFunctionEntrySimpleInterpretedHandler(env, /*nargs=*/i);
    }

    for (word i = 0; i < kMaxNargs; i++) {
      __ align(16);
      __ bind(&env->function_entry_simple_builtin[i]);
      env->register_state.resetTo(env->function_entry_assignment);
      emitFunctionEntryBuiltin(env, /*nargs=*/i);
    }
  }

  __ bind(&env->call_interpreted_slow_path);
  env->register_state.resetTo(env->call_interpreted_slow_path_assignment);
  emitCallInterpretedSlowPath(env);

  __ bind(&env->call_trampoline);
  env->register_state.resetTo(env->call_trampoline_assignment);
  emitCallTrampoline(env);

  // Emit the generic handler stubs at the end, out of the way of the
  // interesting code.
  for (word i = 0; i < 256; ++i) {
    __ bind(&env->opcode_handlers[i]);
    env->register_state.resetTo(env->handler_assignment);
    emitGenericHandler(env, static_cast<Bytecode>(i));
  }
}

class X64Interpreter : public Interpreter {
 public:
  X64Interpreter();
  ~X64Interpreter() override;
  void setupThread(Thread* thread) override;
  void* entryAsm(const Function& function) override;
  void setOpcodeCounting(bool enabled) override;

 private:
  byte* code_;
  word size_;

  void* function_entry_with_intrinsic_;
  void* function_entry_with_no_intrinsic_;
  void* function_entry_simple_interpreted_[kMaxNargs];
  void* function_entry_simple_builtin_[kMaxNargs];

  void* default_handler_table_ = nullptr;
  void* counting_handler_table_ = nullptr;
  bool count_opcodes_ = false;
};

X64Interpreter::X64Interpreter() {
  EmitEnv env;
  emitInterpreter(&env);

  // Finalize the code.
  size_ = env.as.codeSize();
  code_ = OS::allocateMemory(size_, &size_);
  env.as.finalizeInstructions(MemoryRegion(code_, size_));
  OS::protectMemory(code_, size_, OS::kReadExecute);

  // Generate jump targets.
  function_entry_with_intrinsic_ =
      code_ + env.function_entry_with_intrinsic_handler.position();
  function_entry_with_no_intrinsic_ =
      code_ + env.function_entry_with_no_intrinsic_handler.position();
  for (word i = 0; i < kMaxNargs; i++) {
    function_entry_simple_interpreted_[i] =
        code_ + env.function_entry_simple_interpreted_handler[i].position();
  }
  for (word i = 0; i < kMaxNargs; i++) {
    function_entry_simple_builtin_[i] =
        code_ + env.function_entry_simple_builtin[i].position();
  }

  default_handler_table_ = code_ + env.handler_offset;
  counting_handler_table_ = code_ + env.counting_handler_offset;
}

X64Interpreter::~X64Interpreter() { OS::freeMemory(code_, size_); }

void X64Interpreter::setupThread(Thread* thread) {
  thread->setInterpreterFunc(reinterpret_cast<Thread::InterpreterFunc>(code_));
  thread->setInterpreterData(count_opcodes_ ? counting_handler_table_
                                            : default_handler_table_);
}

void X64Interpreter::setOpcodeCounting(bool enabled) {
  count_opcodes_ = enabled;
}

void* X64Interpreter::entryAsm(const Function& function) {
  if (function.intrinsic() != nullptr) {
    return function_entry_with_intrinsic_;
  }
  word argcount = function.argcount();
  if (function.hasSimpleCall() && function.isInterpreted() &&
      argcount < kMaxNargs) {
    CHECK(argcount >= 0, "can't have negative argcount");
    return function_entry_simple_interpreted_[argcount];
  }
  if (function.entry() == builtinTrampoline && function.hasSimpleCall() &&
      argcount < kMaxNargs) {
    DCHECK(function.intrinsic() == nullptr, "expected no intrinsic");
    CHECK(Code::cast(function.code()).code().isSmallInt(),
          "expected SmallInt code");
    return function_entry_simple_builtin_[argcount];
  }
  return function_entry_with_no_intrinsic_;
}

}  // namespace

static void deoptimizeCurrentFunction(Thread* thread) {
  EVENT(DEOPT_FUNCTION);
  Frame* frame = thread->currentFrame();
  // Reset the PC because we're about to jump back into the assembly
  // interpreter and we want to re-try the current opcode.
  frame->setVirtualPC(frame->virtualPC() - kCodeUnitSize);
  HandleScope scope(thread);
  Function function(&scope, frame->function());
  thread->runtime()->populateEntryAsm(function);
  function.setFlags(function.flags() & ~Function::Flags::kCompiled);
}

bool canCompileFunction(Thread* thread, const Function& function) {
  if (!function.isInterpreted()) {
    std::fprintf(
        stderr, "Could not compile '%s' (not interpreted)\n",
        unique_c_ptr<char>(Str::cast(function.qualname()).toCStr()).get());
    return false;
  }
  if (!function.hasSimpleCall()) {
    std::fprintf(
        stderr, "Could not compile '%s' (not simple)\n",
        unique_c_ptr<char>(Str::cast(function.qualname()).toCStr()).get());
    return false;
  }
  if (function.isCompiled()) {
    std::fprintf(
        stderr, "Could not compile '%s' (already compiled)\n",
        unique_c_ptr<char>(Str::cast(function.qualname()).toCStr()).get());
    return false;
  }
  HandleScope scope(thread);
  MutableBytes code(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(code);
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(code, &i);
    if (!isSupportedInJIT(op.bc)) {
      std::fprintf(
          stderr, "Could not compile '%s' (%s)\n",
          unique_c_ptr<char>(Str::cast(function.qualname()).toCStr()).get(),
          kBytecodeNames[op.bc]);
      return false;
    }
  }
  return true;
}

void compileFunction(Thread* thread, const Function& function) {
  EVENT(COMPILE_FUNCTION);
  HandleScope scope(thread);
  MutableBytes code(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(code);
  JitEnv environ(&scope, thread, function, num_opcodes);
  JitEnv* env = &environ;

  // TODO(T89721395): Deduplicate these assignments with emitInterpreter.
  RegisterAssignment function_entry_assignment[] = {
      {&env->pc, kPCReg},
      {&env->oparg, kOpargReg},
      {&env->frame, kFrameReg},
      {&env->thread, kThreadReg},
      {&env->handlers_base, kHandlersBaseReg},
      {&env->callable, kCallableReg},
  };
  env->function_entry_assignment = function_entry_assignment;

  RegisterAssignment handler_assignment[] = {
      {&env->bytecode, kBCReg},   {&env->pc, kPCReg},
      {&env->oparg, kOpargReg},   {&env->frame, kFrameReg},
      {&env->thread, kThreadReg}, {&env->handlers_base, kHandlersBaseReg},
  };
  env->handler_assignment = handler_assignment;

  // Similar to handler_assignment but no PC or oparg.
  RegisterAssignment jit_handler_assignment[] = {
      {&env->bytecode, kBCReg},
      {&env->frame, kFrameReg},
      {&env->thread, kThreadReg},
      {&env->handlers_base, kHandlersBaseReg},
  };
  env->jit_handler_assignment = jit_handler_assignment;

  RegisterAssignment call_interpreted_slow_path_assignment[] = {
      {&env->pc, kPCReg},       {&env->callable, kCallableReg},
      {&env->frame, kFrameReg}, {&env->thread, kThreadReg},
      {&env->oparg, kOpargReg}, {&env->handlers_base, kHandlersBaseReg},
  };
  env->call_interpreted_slow_path_assignment =
      call_interpreted_slow_path_assignment;

  RegisterAssignment call_trampoline_assignment[] = {
      {&env->pc, kPCReg},       {&env->callable, kCallableReg},
      {&env->frame, kFrameReg}, {&env->thread, kThreadReg},
      {&env->oparg, kOpargReg}, {&env->handlers_base, kHandlersBaseReg},
  };
  env->call_trampoline_assignment = call_trampoline_assignment;

  RegisterAssignment return_handler_assignment[] = {
      {&env->thread, kThreadReg},
      {&env->handlers_base, kHandlersBaseReg},
  };
  env->return_handler_assignment = return_handler_assignment;

  RegisterAssignment do_return_assignment[] = {
      {&env->return_value, kReturnRegs[0]},
  };
  env->do_return_assignment = do_return_assignment;

  RegisterAssignment deopt_assignment[] = {
      {&env->thread, kThreadReg},
      {&env->frame, kFrameReg},
      {&env->pc, kPCReg},
  };
  env->deopt_assignment = deopt_assignment;

  DCHECK(function.isInterpreted(), "function must be interpreted");
  DCHECK(function.hasSimpleCall(),
         "function must have a simple calling convention");

  // JIT entrypoints are in entryAsm and are called with the function entry
  // assignment.
  env->register_state.resetTo(function_entry_assignment);

  COMMENT("Function <%s>",
          unique_c_ptr<char>(Str::cast(function.qualname()).toCStr()).get());
  COMMENT("Prologue");
  // Check that we received the right number of arguments.
  Label call_interpreted_slow_path;
  __ cmpl(env->oparg, Immediate(function.argcount()));
  env->register_state.check(env->call_interpreted_slow_path_assignment);
  __ jcc(NOT_EQUAL, &call_interpreted_slow_path, Assembler::kFarJump);

  // Open a new frame.
  env->register_state.assign(&env->return_mode, kReturnModeReg);
  __ xorl(env->return_mode, env->return_mode);
  emitPushCallFrame(env, /*stack_overflow=*/&call_interpreted_slow_path);

  for (word i = 0; i < num_opcodes;) {
    word current_pc = i * kCodeUnitSize;
    BytecodeOp op = nextBytecodeOp(code, &i);
    if (!isSupportedInJIT(op.bc)) {
      UNIMPLEMENTED("unsupported jit opcode %s", kBytecodeNames[op.bc]);
    }
    env->current_op = op.bc;
    env->setCurrentOp(op);
    env->setVirtualPC(i * kCodeUnitSize);
    env->register_state.resetTo(env->jit_handler_assignment);
    COMMENT("%s %d (%d)", kBytecodeNames[op.bc], op.arg, op.cache);
    __ bind(env->opcodeAtByteOffset(current_pc));
    switch (op.bc) {
#define BC(name, _0, _1)                                                       \
  case name: {                                                                 \
    jitEmitHandler<name>(env);                                                 \
    break;                                                                     \
  }
      FOREACH_BYTECODE(BC)
#undef BC
    }
  }

  if (!env->unwind_handler.isUnused()) {
    COMMENT("Unwind");
    __ bind(&env->unwind_handler);
    // TODO(T91715866): Unwind.
    __ ud2();
  }

  COMMENT("Call interpreted slow path");
  __ bind(&call_interpreted_slow_path);
  // TODO(T89721522): Have one canonical slow path chunk of code that all JIT
  // functions jump to, instead of one per function.
  env->register_state.resetTo(env->call_interpreted_slow_path_assignment);
  emitCallInterpretedSlowPath(env);

  if (!env->deopt_handler.isUnused()) {
    COMMENT("Deopt");
    // Handle deoptimization by resetting the entrypoint to an assembly
    // entrypoint and then jumping back into the interpreter.
    __ bind(&env->deopt_handler);
    env->register_state.resetTo(env->deopt_assignment);
    __ movq(kArgRegs[0], env->thread);
    emitSaveInterpreterState(env, kVMPC | kVMStack | kVMFrame);
    emitCall<void (*)(Thread*)>(env, deoptimizeCurrentFunction);
    // Jump back into the interpreter.
    emitRestoreInterpreterState(env, kAllState);
    emitNextOpcodeImpl(env);
  }

  COMMENT("<END>");
  __ ud2();

  // Finalize the code.
  word jit_size = Utils::roundUp(env->as.codeSize(), kBitsPerByte);
  uword address;
  bool allocated =
      thread->runtime()->allocateForMachineCode(jit_size, &address);
  CHECK(allocated, "could not allocate memory for JIT function");
  byte* jit_code = reinterpret_cast<byte*>(address);
  env->as.finalizeInstructions(MemoryRegion(jit_code, jit_size));
  // TODO(T83754516): Mark memory as RX.

  // Replace the entrypoint.
  function.setEntryAsm(jit_code);
  function.setFlags(function.flags() | Function::Flags::kCompiled);
}

Interpreter* createAsmInterpreter() { return new X64Interpreter; }

}  // namespace py
