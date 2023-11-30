// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "bytecode.h"

#include <functional>
#include <vector>

#include "debugging.h"
#include "event.h"
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
  return BytecodeOp{bc, arg, cache};
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
  auto cached_unop = [](Interpreter::UnaryOp unary_op) {
    // TODO(emacs): Add caching for methods on non-smallints
    return RewrittenOp{UNARY_OP_ANAMORPHIC, static_cast<int32_t>(unary_op),
                       false};
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
      // TODO(emacs): Fill in other unary ops
    case UNARY_NEGATIVE:
      return cached_unop(Interpreter::UnaryOp::NEGATIVE);
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
    case BINARY_OP_ANAMORPHIC:
    case COMPARE_OP_ANAMORPHIC:
    case FOR_ITER_ANAMORPHIC:
    case INPLACE_OP_ANAMORPHIC:
    case LOAD_ATTR_ANAMORPHIC:
    case LOAD_FAST_REVERSE:
    case LOAD_METHOD_ANAMORPHIC:
    case STORE_ATTR_ANAMORPHIC:
    case UNARY_OP_ANAMORPHIC:
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

static constexpr uword setBottomNBits(uword n) {
  // Shifting by the word size is undefined behavior in C++.
  return n == kBitsPerWord ? kMaxUword : (1 << n) - 1;
}

static_assert(setBottomNBits(0) == 0, "");
static_assert(setBottomNBits(1) == 1, "");
static_assert(setBottomNBits(2) == 3, "");
static_assert(setBottomNBits(3) == 7, "");
static_assert(setBottomNBits(kBitsPerWord) == kMaxUword, "");

struct Edge {
  word cur_idx;
  word next_idx;
};

#define FOREACH_UNSUPPORTED_CASE(V)                                            \
  V(POP_BLOCK)                                                                 \
  V(SETUP_ASYNC_WITH)                                                          \
  V(SETUP_FINALLY)                                                             \
  V(SETUP_WITH)                                                                \
  V(WITH_CLEANUP_START)                                                        \
  V(YIELD_FROM)                                                                \
  V(YIELD_VALUE)                                                               \
  V(END_ASYNC_FOR)

static Vector<Edge> findEdges(const MutableBytes& bytecode) {
  // TODO(max): Collapse edges for uninteresting opcodes. There shouldn't be
  // edges for POP_TOP, etc; just control flow and anything that touches
  // locals. But maybe this is analysis specific (definite assignment only
  // cares about STORE_FAST and DELETE_FAST whereas constant propagation cares
  // about LOAD_CONST and BINARY_ADD and stuff.)
  Vector<Edge> edges;
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  for (word i = 0; i < num_opcodes;) {
    // Make a copy because nextBytecodeOp modifies the index in-place.
    word cur = i;
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    word next = i;
    switch (op.bc) {
      case JUMP_IF_FALSE_OR_POP:
      case JUMP_IF_TRUE_OR_POP:
      case POP_JUMP_IF_FALSE:
      case POP_JUMP_IF_TRUE:
        edges.push_back(Edge{cur, next});
        edges.push_back(Edge{cur, op.arg / kCompilerCodeUnitSize});
        break;
      case JUMP_FORWARD:
        edges.push_back(Edge{cur, next + op.arg / kCompilerCodeUnitSize});
        break;
      case JUMP_ABSOLUTE:
        edges.push_back(Edge{cur, op.arg / kCompilerCodeUnitSize});
        break;
      case FOR_ITER:
        edges.push_back(Edge{cur, next});
        edges.push_back(Edge{cur, next + op.arg / kCompilerCodeUnitSize});
        break;
#define CASE(op) case op:
        FOREACH_UNSUPPORTED_CASE(CASE)
#undef CASE
        UNIMPLEMENTED("exceptions, generators, and context managers: opcode %s",
                      kBytecodeNames[op.bc]);
        break;
      case RETURN_VALUE:
        // Return exits the function so there is no edge to the next opcode.
        break;
      case RAISE_VARARGS:
        // In the absence of try/except, RAISE_VARARGS exits the function, so
        // there is no edge to the next opcode.
        break;
      default:
        // By default, each instruction "jumps" to the next.
        edges.push_back(Edge{cur, next});
        break;
    }
  }
  return edges;
}

static bool isHardToAnalyze(Thread* thread, const Function& function) {
  uword flags = function.flags();
  if (flags &
      (Function::Flags::kGenerator | Function::Flags::kAsyncGenerator |
       Function::Flags::kCoroutine | Function::Flags::kIterableCoroutine)) {
    return true;
  }
  HandleScope scope(thread);
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    switch (op.bc) {
#define CASE(op) case op:
      FOREACH_UNSUPPORTED_CASE(CASE)
#undef CASE
      DTRACE_PROBE1(python, DefiniteAssignmentBailout, kBytecodeNames[op.bc]);
      return true;
      default:
        break;
    }
  }
  return false;
}


static word runUntilFixpoint(std::function<bool()> f) {
  word num_iterations = 0;
  for (bool changed = true; changed;) {
    DCHECK(num_iterations < 100, "Too many iterations... something went wrong");
    num_iterations++;
    changed = f();
  }
  return num_iterations;
}

template <typename T>
class Lattice {
 public:
  Lattice<T> meet(const Lattice& other) const;
  static Lattice<T> top();
  static Lattice<T> bottom();

 private:
  T value_;
};

enum class DefiniteAssignmentLatticeValue {
  kTop = 0x3,                    // 0b11
  kDefinitelyAssigned = 0x2,     // 0b10
  kDefinitelyNotAssigned = 0x1,  // 0b01
  kBottom = 0x0,                 // 0b00
};

class DefiniteAssignmentLattice
    : public Lattice<DefiniteAssignmentLatticeValue> {
 public:
  DefiniteAssignmentLattice() : value_(DefiniteAssignmentLatticeValue::kTop) {}
  DefiniteAssignmentLattice(DefiniteAssignmentLatticeValue value)
      : value_(value) {}
  DefiniteAssignmentLattice meet(const DefiniteAssignmentLattice& other) const {
    return DefiniteAssignmentLattice{
        static_cast<DefiniteAssignmentLatticeValue>(
            static_cast<uword>(value_) & static_cast<uword>(other.value_))};
  }
  DefiniteAssignmentLatticeValue value() const { return value_; }
  static DefiniteAssignmentLattice top() {
    return DefiniteAssignmentLattice{DefiniteAssignmentLatticeValue::kTop};
  }
  static DefiniteAssignmentLattice bottom() {
    return DefiniteAssignmentLattice{DefiniteAssignmentLatticeValue::kBottom};
  }
  bool isDefinitelyAssigned() const {
    return value_ == DefiniteAssignmentLatticeValue::kDefinitelyAssigned;
  }
  bool isDefinitelyNotAssigned() const {
    return value_ == DefiniteAssignmentLatticeValue::kDefinitelyNotAssigned;
  }
  bool operator==(const DefiniteAssignmentLattice& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const DefiniteAssignmentLattice& other) const {
    return !(*this == other);
  }

 private:
  DefiniteAssignmentLatticeValue value_;
};

template <typename T>
class Locals {
 public:
  Locals(word size) : size_(size) {
    locals_.resize(size);
    for (word i = 0; i < size_; i++) {
      set(i, T::top());
    }
  }
  Locals(const Locals<T>& other) : Locals(other.size_) {
    for (word i = 0; i < size_; i++) {
      set(i, other.get(i));
    }
  }
  Locals<T> operator=(const Locals<T>& other) {
    DCHECK(size_ == other.size_, "Locals must be the same size");
    for (word i = 0; i < size_; i++) {
      set(i, other.get(i));
    }
    return *this;
  }
  void set(word index, T value) {
    DCHECK_INDEX(index, size_);
    locals_[index] = value;
  }
  T get(word index) const {
    DCHECK_INDEX(index, size_);
    return locals_[index];
  }
  bool operator==(const Locals<T>& other) const {
    return size_ == other.size_ && locals_ == other.locals_;
  }
  bool operator!=(const Locals<T>& other) const { return !(*this == other); }
  word size() const { return size_; }

 private:
  word size_{-1};
  std::vector<T> locals_;
};

static void analyzeDefiniteAssignment(Thread* thread, const Function& function,
                                      const Vector<Edge>& edges) {
  HandleScope scope(thread);
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  word total_locals = function.totalLocals();
  // Lattice definition
  auto meet = [](const Locals<DefiniteAssignmentLattice>& left,
                 const Locals<DefiniteAssignmentLattice>& right) {
    DCHECK(left.size() == right.size(), "Locals must be the same size");
    Locals<DefiniteAssignmentLattice> result{left.size()};
    for (word i = 0; i < left.size(); i++) {
      result.set(i, left.get(i).meet(right.get(i)));
    }
    return result;
  };
  // Map of bytecode index to the locals vec representing which locals are
  // definitely assigned.
  std::vector<Locals<DefiniteAssignmentLattice>> defined_in;
  std::vector<Locals<DefiniteAssignmentLattice>> defined_out;
  for (word i = 0; i < num_opcodes; i++) {
    defined_in.emplace_back(Locals<DefiniteAssignmentLattice>(total_locals));
    defined_out.emplace_back(Locals<DefiniteAssignmentLattice>(total_locals));
  }
  // We enter the function with all parameters definitely assigned.
  for (word i = 0; i < function.totalArgs(); i++) {
    defined_in[0].set(i, DefiniteAssignmentLatticeValue::kDefinitelyAssigned);
  }
  // Run until fixpoint.
  word num_iterations =
      runUntilFixpoint([&edges, &defined_in, &bytecode, &defined_out, &meet] {
        bool changed = false;
        for (const Edge& edge : edges) {
          Bytecode op = rewrittenBytecodeOpAt(bytecode, edge.cur_idx);
          uword arg = rewrittenBytecodeArgAt(bytecode, edge.cur_idx);
          const Locals<DefiniteAssignmentLattice>& defined_before =
              defined_in[edge.cur_idx];
          Locals<DefiniteAssignmentLattice> defined_after = defined_before;
          switch (op) {
            case STORE_FAST:
              defined_after.set(
                  arg, DefiniteAssignmentLatticeValue::kDefinitelyAssigned);
              break;
            case DELETE_FAST:
              defined_after.set(
                  arg, DefiniteAssignmentLatticeValue::kDefinitelyNotAssigned);
              break;
            default:
              break;
          }
          if (defined_out[edge.cur_idx] != defined_after) {
            changed = true;
            defined_out[edge.cur_idx] = defined_after;
          }
          Locals<DefiniteAssignmentLattice> next_met =
              meet(defined_in[edge.next_idx], defined_after);
          if (defined_in[edge.next_idx] != next_met) {
            changed = true;
            defined_in[edge.next_idx] = next_met;
          }
        }
        return changed;
      });
  DTRACE_PROBE1(python, DefiniteAssignmentIterations, num_iterations);
  // Rewrite all LOAD_FAST opcodes with definitely-assigned locals to
  // LOAD_FAST_REVERSE_UNCHECKED (if the arg would fit in a byte).
  for (word i = 0; i < num_opcodes; i++) {
    Bytecode op = rewrittenBytecodeOpAt(bytecode, i);
    if (op != LOAD_FAST) {
      continue;
    }
    const Locals<DefiniteAssignmentLattice>& defined_before = defined_in[i];
    uword arg = rewrittenBytecodeArgAt(bytecode, i);
    if (!defined_before.get(arg).isDefinitelyAssigned()) {
      continue;
    }
    // Check if the original opcode uses an extended arg
    if (arg > kMaxByte) {
      DTRACE_PROBE1(python, DefiniteAssignmentBailout,
                    "load_fast_extended_arg");
      continue;
    }
    int32_t reverse_arg = total_locals - arg - 1;
    // Check that the new value fits in a byte
    if (reverse_arg > kMaxByte) {
      DTRACE_PROBE1(python, DefiniteAssignmentBailout, "reverse_arg_too_large");
      continue;
    }
    rewrittenBytecodeOpAtPut(bytecode, i, LOAD_FAST_REVERSE_UNCHECKED);
    rewrittenBytecodeArgAtPut(bytecode, i, reverse_arg);
  }
}

void analyzeBytecode(Thread* thread, const Function& function) {
  DTRACE_PROBE(python, AnalysisAttempt);
  HandleScope scope(thread);
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  Bytecode last_op = rewrittenBytecodeOpAt(bytecode, num_opcodes - 1);
  DCHECK(last_op == RETURN_VALUE, "Last opcode must be RETURN_VALUE (was %s)",
         kBytecodeNames[last_op]);
  word num_locals = Code::cast(function.code()).nlocals();
  if (num_locals == 0) {
    // Nothing to do.
    DTRACE_PROBE1(python, DefiniteAssignmentBailout, "no_locals");
    return;
  }
  if (num_locals > 64) {
    // We don't support more than 64 locals.
    DTRACE_PROBE1(python, DefiniteAssignmentBailout, "too_many_locals");
    return;
  }
  if (isHardToAnalyze(thread, function)) {
    // I don't want to deal with the block stack (yet?).
    return;
  }
  if (num_opcodes == 0) {
    // Some tests generate empty code objects. Bail out.
    return;
  }
  Vector<Edge> edges = findEdges(bytecode);
  analyzeDefiniteAssignment(thread, function, edges);
  DTRACE_PROBE(python, AnalysisSuccess);
}

static const word kMaxCaches = 65536;

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
  analyzeBytecode(thread, function);
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  word cache = num_global_caches;
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    word previous_index = i - 1;
    RewrittenOp rewritten = rewriteOperation(function, op);
    if (rewritten.bc == UNUSED_BYTECODE_0) continue;
    if (rewritten.needs_inline_cache) {
      if (cache < kMaxCaches) {
        rewrittenBytecodeOpAtPut(bytecode, previous_index, rewritten.bc);
        rewrittenBytecodeArgAtPut(bytecode, previous_index,
                                  static_cast<byte>(rewritten.arg));
        rewrittenBytecodeCacheAtPut(bytecode, previous_index, cache);

        cache++;
      }
      continue;
    }
    if (rewritten.arg != op.arg || rewritten.bc != op.bc) {
      rewrittenBytecodeOpAtPut(bytecode, previous_index, rewritten.bc);
      rewrittenBytecodeArgAtPut(bytecode, previous_index,
                                static_cast<byte>(rewritten.arg));
    }
  }
  // It may end up exactly equal to kMaxCaches; that's fine because it's a post
  // increment.
  DCHECK(cache <= kMaxCaches, "Too many caches: %ld", cache);
  if (cache > 0) {
    MutableTuple caches(&scope,
                        runtime->newMutableTuple(cache * kIcPointersPerEntry));
    caches.fill(NoneType::object());
    function.setCaches(*caches);
  }
}

}  // namespace py
