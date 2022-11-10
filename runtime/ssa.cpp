#include "ssa.h"

#include <deque>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "interpreter.h"
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
  V(LoadFast)                                                                  \
  V(BinaryOpSmallInt)                                                          \
  V(Undefined)

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

  virtual std::string toString() const {
    DCHECK(value_.isSmallInt(), "expected small int");
    return typeName() + " " + std::to_string(SmallInt::cast(value_).value());
  }

 private:
  RawObject value_;
};

class LoadFast : public Node {
 public:
  LoadFast(word idx) : Node(NodeType::kLoadFast), idx_(idx) {}

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

void ssaify(Thread* thread, const Function& function) {
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
    local_nodes[i] = env.emit<LoadFast>(i);
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
        Node* result = stack_nodes.back();
        std::string graph = graphviz(result);
        fprintf(stderr, "digraph Function {\n%s}\n", graph.c_str());
        stack_nodes.pop_back();
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
      default: {
        UNREACHABLE("Unsupported opcode %s in ssaify", kBytecodeNames[op.bc]);
        break;
      }
    }
  }
}

}  // namespace py
