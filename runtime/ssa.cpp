#include "ssa.h"

#include <deque>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "bytearray-builtins.h"
#include "interpreter.h"
#include "runtime.h"
#include "utils.h"

namespace py {

class BumpAllocator {
 public:
  BumpAllocator(size_t size) {
    raw_ = OS::allocateMemory(size, &actual_size_);
    fill_ = reinterpret_cast<uintptr_t>(raw_);
    end_ = fill_ + size;
  }

  ~BumpAllocator() { OS::freeMemory(raw_, actual_size_); }

  template <typename T, typename... Args>
  T* allocate(Args&&... args) {
    uintptr_t fill = fill_;
    uintptr_t free = end_ - fill;
    size_t element_size_ = Utils::roundUp(sizeof(T), alignof(T));
    if (element_size_ > free) {
      return nullptr;
    }
    fill_ = fill + element_size_;
    T* mem = reinterpret_cast<T*>(fill);
    return new (mem) T(std::forward<Args>(args)...);
  }

  uintptr_t fill() { return fill_; }

  uintptr_t size() { return actual_size_; }

 private:
  uword end_{0};
  uword fill_{0};
  word actual_size_{0};
  byte* raw_{nullptr};
};

#define FOREACH_NODE(V)                                                        \
  V(Immediate)                                                                 \
  V(LoadArg)                                                                   \
  V(LoadFast)                                                                  \
  V(BinaryOpSmallInt)                                                          \
  V(Undefined)                                                                 \
  V(BasicBlock)

enum class NodeType {
#define ENUM(name) k##name,
  FOREACH_NODE(ENUM)
#undef ENUM
};

static const char* kNodeTypeNames[] = {
#define STR(name) #name,
    FOREACH_NODE(STR)
#undef STR
};

class Node;

#define DECLARE(name) class name;
FOREACH_NODE(DECLARE)
#undef DECLARE

class NodeVisitor {
 public:
  virtual void visitEdge(Node* from, Node* to) = 0;
  virtual ~NodeVisitor() = default;
};

class Node {
 public:
  Node(NodeType type) : type_(type) {}
  virtual void visitEdges(NodeVisitor*){};
  word id() const { return id_; }
  void setId(word id) { id_ = id; }
  NodeType type() const { return type_; }
  std::string typeName() const {
    return kNodeTypeNames[static_cast<word>(type())];
  }
  virtual std::string toString() const { return typeName(); };

#define PRED(name)                                                             \
  bool is##name() const { return type() == NodeType::k##name; }
  FOREACH_NODE(PRED)
#undef PRED

#define CAST(name)                                                             \
  name* as##name() {                                                           \
    DCHECK(is##name(), "bad cast");                                            \
    return reinterpret_cast<name*>(this);                                      \
  }
  FOREACH_NODE(CAST)
#undef CAST

 private:
  word id_{0};
  NodeType type_;
};

class Undefined : public Node {
 public:
  Undefined() : Node(NodeType::kUndefined) {}
};

class Immediate : public Node {
 public:
  Immediate(RawObject value) : Node(NodeType::kImmediate), value_(value) {
    DCHECK(!value.isHeapObject(), "constant value must be immediate");
  }

  RawObject value() const { return value_; }

  virtual std::string toString() const {
    DCHECK(value_.isSmallInt(), "expected small int");
    return typeName() + " " + std::to_string(SmallInt::cast(value_).value());
  }

 private:
  RawObject value_;
};

class LoadArg : public Node {
 public:
  LoadArg(word idx) : Node(NodeType::kLoadArg), idx_(idx) {}

  word idx() const { return idx_; }

  virtual std::string toString() const {
    return typeName() + " " + std::to_string(idx_);
  }

 private:
  word idx_{-1};
};

class LoadFast : public Node {
 public:
  LoadFast(word idx) : Node(NodeType::kLoadFast), idx_(idx) {}

  word idx() const { return idx_; }

  virtual std::string toString() const {
    return typeName() + " " + std::to_string(idx_);
  }

 private:
  word idx_{-1};
};

// TODO(max): Expose from interpreter or something
static const SymbolId kBinaryOperationSelector[] = {
    ID(__add__),     ID(__sub__),      ID(__mul__),    ID(__matmul__),
    ID(__truediv__), ID(__floordiv__), ID(__mod__),    ID(__divmod__),
    ID(__pow__),     ID(__lshift__),   ID(__rshift__), ID(__and__),
    ID(__xor__),     ID(__or__)};

class BinaryOpSmallInt : public Node {
 public:
  BinaryOpSmallInt(Interpreter::BinaryOp op, Node* left, Node* right)
      : Node(NodeType::kBinaryOpSmallInt),
        op_(op),
        left_(left),
        right_(right) {}

  Interpreter::BinaryOp op() const { return op_; }

  Node* left() const { return left_; }

  Node* right() const { return right_; }

  virtual std::string toString() const {
    return Symbols::predefinedSymbolAt(
        kBinaryOperationSelector[static_cast<word>(op_)]);
  }

  void visitEdges(NodeVisitor* visitor) {
    visitor->visitEdge(this, left_);
    visitor->visitEdge(this, right_);
  }

 private:
  Interpreter::BinaryOp op_;
  Node* left_{nullptr};
  Node* right_{nullptr};
};

class BasicBlock : public Node {
 public:
  BasicBlock(NodeType type, Node* body) : Node(type), body_(body) {}

  Node* body() const { return body_; }

 private:
  Node* body_{nullptr};
};

class Return : public BasicBlock {
 public:
  Return(Node* body) : BasicBlock(NodeType::kReturn, body) {}
};

// class Branch : public BasicBlock {
//   Branch(Node* body, BasicBlock* target) : Node(NodeType::kBranch),
//   BasicBlock(body), target_(target) {}
// };

class CondBranch : public BasicBlock {
 public:
  CondBranch(Node* body, BasicBlock* iftrue, BasicBlock* iffalse)
      : BasicBlock(NodeType::kReturn), iftrue_(iftrue), iffalse_(iffalse) {}

 private:
  BasicBlock* iftrue_{false};
  BasicBlock iffalse_{false};
};

class Env {
 public:
  template <typename T, typename... Args>
  T* emit(Args&&... args) {
    T* result = allocator_.allocate<T>(std::forward<Args>(args)...);
    result->setId(next_id_++);
    return result;
  }

 private:
  BumpAllocator allocator_{1 * kKiB};
  word next_id_{0};
};

static std::string graphviz(Node* root) {
  std::unordered_set<Node*> visited;
  std::deque<Node*> worklist;
  worklist.push_back(root);
  class GraphvizVisitor : public NodeVisitor {
   public:
    GraphvizVisitor(std::deque<Node*>* worklist, std::stringstream* result)
        : worklist_(worklist), result_(result) {}
    void visitEdge(Node* from, Node* to) {
      *result_ << from->id() << " -> " << to->id() << ";\n";
      worklist_->push_back(to);
    }

   private:
    std::deque<Node*>* worklist_;
    std::stringstream* result_;
  };
  std::stringstream result;
  GraphvizVisitor visitor(&worklist, &result);
  while (!worklist.empty()) {
    Node* node = worklist.front();
    worklist.pop_front();
    if (visited.count(node)) {
      continue;
    }
    result << node->id() << " [label=\"" << node->toString() << "\"];\n";
    visited.insert(node);
    node->visitEdges(&visitor);
  }
  return result.str();
}

static RawObject compileNode(Thread* thread, const Function& function,
                             const Bytearray& bytecode, Node* node) {
  auto emit = [&thread, &bytecode](byte op, byte arg) {
    // 0s are for cache idx
    byte code[] = {op, arg, 0, 0};
    thread->runtime()->bytearrayExtend(thread, bytecode, code);
    return NoneType::object();
  };
  if (node->isImmediate()) {
    return emit(LOAD_IMMEDIATE, opargFromObject(node->asImmediate()->value()));
  }
  if (node->isBinaryOpSmallInt()) {
    BinaryOpSmallInt* instr = node->asBinaryOpSmallInt();
    RawObject result = compileNode(thread, function, bytecode, instr->left());
    if (result.isError()) {
      return result;
    }
    result = compileNode(thread, function, bytecode, instr->right());
    if (result.isError()) {
      return result;
    }
    if (instr->op() == Interpreter::BinaryOp::ADD) {
      return emit(BINARY_ADD_SMALLINT, 0);
    }
    if (instr->op() == Interpreter::BinaryOp::SUB) {
      return emit(BINARY_SUB_SMALLINT, 0);
    }
    if (instr->op() == Interpreter::BinaryOp::MUL) {
      return emit(BINARY_MUL_SMALLINT, 0);
    }
    if (instr->op() == Interpreter::BinaryOp::FLOORDIV) {
      return emit(BINARY_FLOORDIV_SMALLINT, 0);
    }
    return thread->raiseWithFmt(LayoutId::kNotImplementedError,
                                "cannot compile %s", node->toString().c_str());
  }
  if (node->isLoadArg()) {
    int32_t reverse_arg = function.totalLocals() - node->asLoadArg()->idx() - 1;
    DCHECK(reverse_arg <= kMaxByte, "cannot fit arg in byte");
    return emit(LOAD_FAST_REVERSE_UNCHECKED, reverse_arg);
  }
  return thread->raiseWithFmt(LayoutId::kNotImplementedError,
                              "cannot compile cfg %s",
                              node->toString().c_str());
}

static RawObject schedule(Thread* thread, const Function& function, Node* cfg) {
  HandleScope scope(thread);
  Bytearray bytecode(&scope, thread->runtime()->newBytearray());
  Object compile_result(&scope, compileNode(thread, function, bytecode, cfg));
  if (compile_result.isError()) {
    return *compile_result;
  }
  byte code[] = {RETURN_VALUE, 0, 0, 0};
  thread->runtime()->bytearrayExtend(thread, bytecode, code);
  MutableBytes result(&scope, thread->runtime()->newMutableBytesUninitialized(
                                  bytecode.numItems()));
  bytecode.copyTo(reinterpret_cast<byte*>(result.address()),
                  bytecode.numItems());
  return *result;
}

RawObject ssaify(Thread* thread, const Function& function) {
  HandleScope scope(thread);
  Code code(&scope, function.code());
  Tuple consts(&scope, code.consts());
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  word num_opcodes = rewrittenBytecodeLength(bytecode);
  std::vector<Node*> const_nodes(consts.length());
  std::vector<Node*> stack_nodes(code.stacksize());
  std::vector<Node*> local_nodes(function.totalLocals());
  Env env;
  for (word i = 0; i < function.totalArgs(); i++) {
    local_nodes[i] = env.emit<LoadArg>(i);
  }
  for (word i = function.totalArgs(); i < function.totalLocals(); i++) {
    local_nodes[i] = env.emit<Undefined>();
  }
  for (word i = 0; i < num_opcodes;) {
    BytecodeOp op = nextBytecodeOp(bytecode, &i);
    switch (op.bc) {
      case LOAD_IMMEDIATE: {
        RawObject obj = objectFromOparg(op.arg);
        stack_nodes.push_back(env.emit<Immediate>(obj));
        break;
      }
      case RETURN_VALUE: {
        Node* value = stack_nodes.back();
        stack_nodes.pop_back();
        stack_nodes.push_back(env.emit<BasicBlock>(value));
        break;
      };
      case LOAD_FAST_REVERSE:
      case LOAD_FAST_REVERSE_UNCHECKED: {
        stack_nodes.push_back(
            local_nodes.at(function.totalLocals() - op.arg - 1));
        break;
      };
      case STORE_FAST_REVERSE: {
        Node* right = stack_nodes.back();
        stack_nodes.pop_back();
        local_nodes.at(function.totalLocals() - op.arg - 1) = right;
        break;
      };
      case BINARY_ADD_SMALLINT: {
        Node* right = stack_nodes.back();
        stack_nodes.pop_back();
        Node* left = stack_nodes.back();
        stack_nodes.pop_back();
        stack_nodes.push_back(env.emit<BinaryOpSmallInt>(
            Interpreter::BinaryOp::ADD, left, right));
        break;
      }
      case BINARY_MUL_SMALLINT: {
        Node* right = stack_nodes.back();
        stack_nodes.pop_back();
        Node* left = stack_nodes.back();
        stack_nodes.pop_back();
        stack_nodes.push_back(env.emit<BinaryOpSmallInt>(
            Interpreter::BinaryOp::MUL, left, right));
        break;
      }
      case POP_JUMP_IF_FALSE: {
        Node* cond = stack_nodes.back();
        stack_nodes.pop_back();
      }
      default: {
        UNREACHABLE("Unsupported opcode %s in ssaify", kBytecodeNames[op.bc]);
        break;
      }
    }
  }
done:
  Node* node = stack_nodes.back();
  stack_nodes.pop_back();
  std::string graph = graphviz(node);
  fprintf(stderr, "digraph Function {\n%s}\n", graph.c_str());
  // Object result(&scope, schedule(thread, function, node));
  // if (result.isError()) {
  //   return *result;
  // }
  // function.setRewrittenBytecode(*result);
  return NoneType::object();
}

}  // namespace py
