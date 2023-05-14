// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "bytecode.h"

#include "ic.h"
#include "runtime.h"

namespace py {

const char* const kBytecodeNames[] = {
#define NAME(name, value, handler) #name,
    FOREACH_BYTECODE(NAME)
#undef NAME
};

BytecodeOp nextBytecodeOp(const MutableBytes& bytecode, word* index) {
  word i = *index;
  Bytecode bc = rewrittenBytecodeOpAt(bytecode, i);
  int32_t arg = rewrittenBytecodeArgAt(bytecode, i);
  uint16_t cache = rewrittenBytecodeCacheAt(bytecode, i);
  i++;
  while (bc == Bytecode::EXTENDED_ARG) {
    bc = rewrittenBytecodeOpAt(bytecode, i);
    arg = (arg << kBitsPerByte) | rewrittenBytecodeArgAt(bytecode, i);
    i++;
  }
  DCHECK(i - *index <= 8, "EXTENDED_ARG-encoded arg must fit in int32_t");
  *index = i;
  return BytecodeOp{bc, arg, cache, i - 1};
}

const word kOpcodeOffset = 0;
const word kArgOffset = 1;
const word kCacheOffset = 2;

word bytecodeLength(const Bytes& bytecode) {
  return bytecode.length() / kCompilerCodeUnitSize;
}

Bytecode bytecodeOpAt(const Bytes& bytecode, word index) {
  return static_cast<Bytecode>(
      bytecode.byteAt(index * kCompilerCodeUnitSize + kOpcodeOffset));
}

byte bytecodeArgAt(const Bytes& bytecode, word index) {
  return bytecode.byteAt(index * kCompilerCodeUnitSize + kArgOffset);
}

word rewrittenBytecodeLength(const MutableBytes& bytecode) {
  return bytecode.length() / kCodeUnitSize;
}

Bytecode rewrittenBytecodeOpAt(const MutableBytes& bytecode, word index) {
  return static_cast<Bytecode>(
      bytecode.byteAt(index * kCodeUnitSize + kOpcodeOffset));
}

void rewrittenBytecodeOpAtPut(const MutableBytes& bytecode, word index,
                              Bytecode op) {
  bytecode.byteAtPut(index * kCodeUnitSize + kOpcodeOffset,
                     static_cast<byte>(op));
}

byte rewrittenBytecodeArgAt(const MutableBytes& bytecode, word index) {
  return bytecode.byteAt(index * kCodeUnitSize + kArgOffset);
}

void rewrittenBytecodeArgAtPut(const MutableBytes& bytecode, word index,
                               byte arg) {
  bytecode.byteAtPut(index * kCodeUnitSize + kArgOffset, arg);
}

uint16_t rewrittenBytecodeCacheAt(const MutableBytes& bytecode, word index) {
  return bytecode.uint16At(index * kCodeUnitSize + kCacheOffset);
}

void rewrittenBytecodeCacheAtPut(const MutableBytes& bytecode, word index,
                                 uint16_t cache) {
  bytecode.uint16AtPut(index * kCodeUnitSize + kCacheOffset, cache);
}

int8_t opargFromObject(RawObject object) {
  DCHECK(!object.isHeapObject(), "Heap objects are disallowed");
  return static_cast<int8_t>(object.raw());
}

struct RewrittenOp {
  Bytecode bc;
  int32_t arg;
  bool needs_inline_cache;
};

static RewrittenOp rewriteOperation(const Function& function, BytecodeOp op) {
  auto cached_binop = [](Interpreter::BinaryOp bin_op) {
    return RewrittenOp{BINARY_OP_ANAMORPHIC, static_cast<int32_t>(bin_op),
                       true};
  };
  auto cached_inplace = [](Interpreter::BinaryOp bin_op) {
    return RewrittenOp{INPLACE_OP_ANAMORPHIC, static_cast<int32_t>(bin_op),
                       true};
  };
  switch (op.bc) {
    case BINARY_ADD:
      return cached_binop(Interpreter::BinaryOp::ADD);
    case BINARY_AND:
      return cached_binop(Interpreter::BinaryOp::AND);
    case BINARY_FLOOR_DIVIDE:
      return cached_binop(Interpreter::BinaryOp::FLOORDIV);
    case BINARY_LSHIFT:
      return cached_binop(Interpreter::BinaryOp::LSHIFT);
    case BINARY_MATRIX_MULTIPLY:
      return cached_binop(Interpreter::BinaryOp::MATMUL);
    case BINARY_MODULO:
      return cached_binop(Interpreter::BinaryOp::MOD);
    case BINARY_MULTIPLY:
      return cached_binop(Interpreter::BinaryOp::MUL);
    case BINARY_OR:
      return cached_binop(Interpreter::BinaryOp::OR);
    case BINARY_POWER:
      return cached_binop(Interpreter::BinaryOp::POW);
    case BINARY_RSHIFT:
      return cached_binop(Interpreter::BinaryOp::RSHIFT);
    case BINARY_SUBSCR:
      return RewrittenOp{BINARY_SUBSCR_ANAMORPHIC, op.arg, true};
    case BINARY_SUBTRACT:
      return cached_binop(Interpreter::BinaryOp::SUB);
    case BINARY_TRUE_DIVIDE:
      return cached_binop(Interpreter::BinaryOp::TRUEDIV);
    case BINARY_XOR:
      return cached_binop(Interpreter::BinaryOp::XOR);
    case COMPARE_OP:
      switch (op.arg) {
        case CompareOp::LT:
        case CompareOp::LE:
        case CompareOp::EQ:
        case CompareOp::NE:
        case CompareOp::GT:
        case CompareOp::GE:
          return RewrittenOp{COMPARE_OP_ANAMORPHIC, op.arg, true};
        case CompareOp::IN:
          return RewrittenOp{COMPARE_IN_ANAMORPHIC, 0, true};
        // TODO(T61327107): Implement COMPARE_NOT_IN.
        case CompareOp::IS:
          return RewrittenOp{COMPARE_IS, 0, false};
        case CompareOp::IS_NOT:
          return RewrittenOp{COMPARE_IS_NOT, 0, false};
      }
      break;
    case CALL_FUNCTION:
      return RewrittenOp{CALL_FUNCTION_ANAMORPHIC, op.arg, true};
    case FOR_ITER:
      return RewrittenOp{FOR_ITER_ANAMORPHIC, op.arg, true};
    case INPLACE_ADD:
      return cached_inplace(Interpreter::BinaryOp::ADD);
    case INPLACE_AND:
      return cached_inplace(Interpreter::BinaryOp::AND);
    case INPLACE_FLOOR_DIVIDE:
      return cached_inplace(Interpreter::BinaryOp::FLOORDIV);
    case INPLACE_LSHIFT:
      return cached_inplace(Interpreter::BinaryOp::LSHIFT);
    case INPLACE_MATRIX_MULTIPLY:
      return cached_inplace(Interpreter::BinaryOp::MATMUL);
    case INPLACE_MODULO:
      return cached_inplace(Interpreter::BinaryOp::MOD);
    case INPLACE_MULTIPLY:
      return cached_inplace(Interpreter::BinaryOp::MUL);
    case INPLACE_OR:
      return cached_inplace(Interpreter::BinaryOp::OR);
    case INPLACE_POWER:
      return cached_inplace(Interpreter::BinaryOp::POW);
    case INPLACE_RSHIFT:
      return cached_inplace(Interpreter::BinaryOp::RSHIFT);
    case INPLACE_SUBTRACT:
      return cached_inplace(Interpreter::BinaryOp::SUB);
    case INPLACE_TRUE_DIVIDE:
      return cached_inplace(Interpreter::BinaryOp::TRUEDIV);
    case INPLACE_XOR:
      return cached_inplace(Interpreter::BinaryOp::XOR);
    case LOAD_ATTR:
      return RewrittenOp{LOAD_ATTR_ANAMORPHIC, op.arg, true};
    case LOAD_FAST: {
      CHECK(op.arg < Code::cast(function.code()).nlocals(),
            "unexpected local number");
      word total_locals = function.totalLocals();
      // Check if the original opcode uses an extended arg
      if (op.arg > kMaxByte) {
        break;
      }
      int32_t reverse_arg = total_locals - op.arg - 1;
      // Check that the new value fits in a byte
      if (reverse_arg > kMaxByte) {
        break;
      }
      return RewrittenOp{LOAD_FAST_REVERSE, reverse_arg, false};
    }
    case LOAD_METHOD:
      return RewrittenOp{LOAD_METHOD_ANAMORPHIC, op.arg, true};
    case STORE_ATTR:
      return RewrittenOp{STORE_ATTR_ANAMORPHIC, op.arg, true};
    case STORE_FAST: {
      CHECK(op.arg < Code::cast(function.code()).nlocals(),
            "unexpected local number");
      // Check if the original opcode uses an extended arg
      if (op.arg > kMaxByte) {
        break;
      }
      word total_locals = function.totalLocals();
      int32_t reverse_arg = total_locals - op.arg - 1;
      // Check that the new value fits in a byte
      if (reverse_arg > kMaxByte) {
        break;
      }
      return RewrittenOp{STORE_FAST_REVERSE, reverse_arg, false};
    }
    case STORE_SUBSCR:
      return RewrittenOp{STORE_SUBSCR_ANAMORPHIC, op.arg, true};
    case LOAD_CONST: {
      RawObject arg_obj =
          Tuple::cast(Code::cast(function.code()).consts()).at(op.arg);
      if (!arg_obj.isHeapObject()) {
        if (arg_obj.isBool()) {
          // We encode true/false not as 1/0 but as 0x80/0 to save an x86
          // assembly instruction; moving the value to the 2nd byte can be done
          // with a multiplication by 2 as part of an address expression rather
          // than needing a separate shift by 8 in the 1/0 variant.
          return RewrittenOp{LOAD_BOOL, Bool::cast(arg_obj).value() ? 0x80 : 0,
                             false};
        }
        // This condition is true only the object fits in a byte. Some
        // immediate values of SmallInt and SmallStr do not satify this
        // condition.
        if (arg_obj == objectFromOparg(opargFromObject(arg_obj))) {
          return RewrittenOp{LOAD_IMMEDIATE, opargFromObject(arg_obj), false};
        }
      }
    } break;
    case BUILD_SLICE: {
      DCHECK(op.arg == 2 || op.arg == 3,
             "BUILD_SLICE oparg must be 2 or 3; found %d", op.arg);
      Thread* thread = Thread::current();
      HandleScope scope(thread);
      Bytes bytecode(&scope, Code::cast(function.code()).code());
      if (op.index > 4 &&
          bytecodeOpAt(bytecode, op.index - 4) == EXTENDED_ARG) {
        // LOAD_CONST, LOAD_CONST, LOAD_CONST, BUILD_SLICE
        break;
      }
      if (bytecodeOpAt(bytecode, op.index - 2) != LOAD_CONST) {
        break;
      }
      if (bytecodeOpAt(bytecode, op.index - 1) != LOAD_CONST) {
        break;
      }
      if (op.arg == 3 && bytecodeOpAt(bytecode, op.index - 3) != LOAD_CONST) {
        break;
      }
      return RewrittenOp{LOAD_SLICE_CACHED, op.arg, true};
    }
    case BINARY_OP_ANAMORPHIC:
    case COMPARE_OP_ANAMORPHIC:
    case FOR_ITER_ANAMORPHIC:
    case INPLACE_OP_ANAMORPHIC:
    case LOAD_ATTR_ANAMORPHIC:
    case LOAD_FAST_REVERSE:
    case LOAD_METHOD_ANAMORPHIC:
    case STORE_ATTR_ANAMORPHIC:
      UNREACHABLE("should not have cached opcode in input");
    default:
      break;
  }
  return RewrittenOp{UNUSED_BYTECODE_0, 0, false};
}

RawObject expandBytecode(Thread* thread, const Bytes& bytecode) {
  // Bytecode comes in in (OP, ARG) pairs. Bytecode goes out in (OP, ARG,
  // CACHE, CACHE) four-tuples.
  HandleScope scope(thread);
  word num_opcodes = bytecodeLength(bytecode);
  MutableBytes result(&scope, thread->runtime()->newMutableBytesUninitialized(
                                  num_opcodes * kCodeUnitSize));
  for (word i = 0; i < num_opcodes; i++) {
    rewrittenBytecodeOpAtPut(result, i, bytecodeOpAt(bytecode, i));
    rewrittenBytecodeArgAtPut(result, i, bytecodeArgAt(bytecode, i));
    rewrittenBytecodeCacheAtPut(result, i, 0);
  }
  return *result;
}

static const word kMaxCaches = 65536;

static RawObject constAtOp(const MutableBytes& bytecode, const Tuple& consts,
                           word index) {
  Bytecode bc = rewrittenBytecodeOpAt(bytecode, index);
  switch (bc) {
    case LOAD_CONST:
      return consts.at(rewrittenBytecodeArgAt(bytecode, index));
    case LOAD_BOOL:
      return Bool::fromBool(rewrittenBytecodeArgAt(bytecode, index));
    case LOAD_IMMEDIATE:
      return objectFromOparg(rewrittenBytecodeArgAt(bytecode, index));
    default:
      UNIMPLEMENTED("unexpected opcode %s", kBytecodeNames[bc]);
  }
}

void rewriteBytecode(Thread* thread, const Function& function) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  // Add cache entries for global variables.
  // TODO(T58223091): This is going to over allocate somewhat in order
  // to simplify the indexing arithmetic.  Not all names are used for
  // globals, some are used for attributes.  This is good enough for
  // now.
  word names_length = Tuple::cast(Code::cast(function.code()).names()).length();
  word num_global_caches = Utils::roundUpDiv(names_length, kIcPointersPerEntry);
  if (!function.hasOptimizedOrNewlocals()) {
    if (num_global_caches > 0) {
      MutableTuple caches(&scope, runtime->newMutableTuple(
                                      num_global_caches * kIcPointersPerEntry));
      caches.fill(NoneType::object());
      function.setCaches(*caches);
    }
    return;
  }
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  // Scan bytecode to figure out how many caches we need and if we can use
  // LOAD_FAST_REVERSE_UNCHECKED.
  word num_caches = num_global_caches;
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    RewrittenOp rewritten = rewriteOperation(function, op);
    if (rewritten.needs_inline_cache) {
      num_caches++;
    }
  }
  if (num_caches > kMaxCaches) {
    // Populate global variable caches unconditionally since the interpreter
    // assumes their existence.
    if (num_global_caches > 0) {
      MutableTuple caches(&scope, runtime->newMutableTuple(
                                      num_global_caches * kIcPointersPerEntry));
      caches.fill(NoneType::object());
      function.setCaches(*caches);
    }
    return;
  }
  word cache = num_global_caches;
  bool rewrite_build_slice = false;
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    word previous_index = i - 1;
    RewrittenOp rewritten = rewriteOperation(function, op);
    if (rewritten.bc == UNUSED_BYTECODE_0) continue;
    if (rewritten.needs_inline_cache) {
      if (rewritten.bc == LOAD_SLICE_CACHED) {
        rewrite_build_slice = true;
      }
      rewrittenBytecodeOpAtPut(bytecode, previous_index, rewritten.bc);
      rewrittenBytecodeArgAtPut(bytecode, previous_index,
                                static_cast<byte>(rewritten.arg));
      rewrittenBytecodeCacheAtPut(bytecode, previous_index, cache);

      cache++;
    } else if (rewritten.arg != op.arg || rewritten.bc != op.bc) {
      rewrittenBytecodeOpAtPut(bytecode, previous_index, rewritten.bc);
      rewrittenBytecodeArgAtPut(bytecode, previous_index,
                                static_cast<byte>(rewritten.arg));
    }
  }
  DCHECK(cache == num_caches, "cache size mismatch");
  if (cache > 0) {
    MutableTuple caches(&scope,
                        runtime->newMutableTuple(cache * kIcPointersPerEntry));
    caches.fill(NoneType::object());
    function.setCaches(*caches);

    if (rewrite_build_slice) {
      // Rewrite LOAD_CONSTs before BUILD_SLICE. We need not worry about
      // EXTENDED_ARG in between because we checked above; we can directly index
      // backwards.
      Tuple consts(&scope, Code::cast(function.code()).consts());
      Object start(&scope, NoneType::object());
      Object stop(&scope, NoneType::object());
      Object step(&scope, NoneType::object());
      Slice slice(&scope, runtime->emptySlice());
      for (word i = 0; i < num_opcodes;) {
        BytecodeOp op = nextBytecodeOp(bytecode, &i);
        if (op.bc == LOAD_SLICE_CACHED) {
          if (op.arg == 2) {
            start = constAtOp(bytecode, consts, op.index - 2);
            stop = constAtOp(bytecode, consts, op.index - 1);
            step = NoneType::object();
          } else {
            start = constAtOp(bytecode, consts, op.index - 3);
            stop = constAtOp(bytecode, consts, op.index - 2);
            step = constAtOp(bytecode, consts, op.index - 1);
            rewrittenBytecodeOpAtPut(bytecode, op.index - 3, NOP);
          }
          rewrittenBytecodeOpAtPut(bytecode, op.index - 1, NOP);
          rewrittenBytecodeOpAtPut(bytecode, op.index - 2, NOP);
          slice = runtime->newSlice(start, stop, step);
          word index = op.cache * kIcPointersPerEntry;
          caches.atPut(index + kIcEntryValueOffset, *slice);
        }
      }
    }
  }
}

}  // namespace py
