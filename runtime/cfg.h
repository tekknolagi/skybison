/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once

#include <unordered_map>

#include "handles-decl.h"
#include "handles.h"
#include "stack.h"
#include "thread.h"

namespace py {

namespace {
template <typename K, typename V>
using Map = std::unordered_map<K, V>;
}

class Register {
 public:
  Register(uword id) : id_(id) {}
  uword id() const { return id_; }

 private:
  uword id_;

  DISALLOW_COPY_AND_ASSIGN(Register);
};

class Instruction;

class BasicBlock {
 public:
  BasicBlock(uword id) : id_(id) {}
  uword id() const { return id_; }
  void emit(Instruction* instr) { instrs_.push_back(instr); }

 private:
  uword id_;
  Vector<Instruction*> instrs_;

  DISALLOW_COPY_AND_ASSIGN(BasicBlock);
};

class BytecodeInstruction : public BytecodeOp {
 public:
  BytecodeInstruction(BytecodeOp op, word offset)
      : BytecodeOp(op), offset_(offset) {}
  word offset() const { return offset_; }
  word index() const { return offset() / kCodeUnitSize; }

  bool isTerminator() const { return isBranch() || isReturn(); }
  bool isBranch() const {
    switch (bc) {
      case FOR_ITER:
      case JUMP_ABSOLUTE:
      case JUMP_FORWARD:
      case JUMP_IF_FALSE_OR_POP:
      case JUMP_IF_TRUE_OR_POP:
      case POP_JUMP_IF_FALSE:
      case POP_JUMP_IF_TRUE:
        return true;
      default:
        return false;
    }
  }
  word jumpTarget() const {
    if (isRelativeBranch()) {
      return nextInstrOffset() + arg;
    }
    return arg;
  }
  bool isRelativeBranch() const { return bc == FOR_ITER || bc == JUMP_FORWARD; }
  bool isReturn() const { return bc == RETURN_VALUE; }
  word nextInstrIdx() const { return nextInstrOffset() / kCodeUnitSize; }
  // TODO(max): Delete this and use indexing into the Vector
  word nextInstrOffset() const { return offset() + kCodeUnitSize; }

 private:
  word offset_;

  DISALLOW_HEAP_ALLOCATION();
};

class BytecodeView {
 public:
  BytecodeView(const MutableBytes& rewritten_bytecode) {
    word num_opcodes = rewrittenBytecodeLength(rewritten_bytecode);
    for (word i = 0; i < num_opcodes;) {
      BytecodeOp op = nextBytecodeOp(rewritten_bytecode, &i);
      instructions_.push_back({op, i * kCodeUnitSize});
    }
  }
  BytecodeInstruction instrAt(word idx) const { return instructions_[idx]; }
  word numInstructions() const { return instructions_.size(); }

 private:
  Vector<BytecodeInstruction> instructions_;

  DISALLOW_HEAP_ALLOCATION();
  // DISALLOW_COPY_AND_ASSIGN(BytecodeView);
};

class BytecodeInstructionBlock {
 public:
  BytecodeInstructionBlock(const BytecodeView& bytecode)
      : bytecode_(bytecode),
        start_idx_(0),
        end_idx_(bytecode.numInstructions()) {}
  BytecodeInstructionBlock(const BytecodeView& bytecode, word start_idx,
                           word end_idx)
      : bytecode_(bytecode), start_idx_(start_idx), end_idx_(end_idx) {}
  const BytecodeView& bytecode() const { return bytecode_; }
  word size() const { return end_idx_ - start_idx_; }

 private:
  BytecodeView bytecode_;
  word start_idx_;
  word end_idx_;
};

class BlockMap {
 public:
  void addBlock(word start_idx, word end_idx, BasicBlock* block,
                const BytecodeView& bytecode) {
    word start_offset = start_idx * kCodeUnitSize;
    blocks_.insert({start_offset, block});
    BytecodeInstructionBlock* bc_block =
        new BytecodeInstructionBlock(bytecode, start_idx, end_idx);
    bc_blocks_.insert({block, bc_block});
  }

  BasicBlock* blockAtOffset(word offset) { return blocks_[offset]; }

 private:
  // Map of bytecode start offsets to basic blocks
  Map<word, BasicBlock*> blocks_;
  // Map of basic blocks to chunks of bytecode
  Map<BasicBlock*, BytecodeInstructionBlock*> bc_blocks_;

  DISALLOW_COPY_AND_ASSIGN(BlockMap);
};

class CFG {
 public:
  CFG() {}
  ~CFG();

  BasicBlock* allocateBlock();
  Register* allocateRegister();

  word numBlocks() const { return blocks_.size(); }
  BasicBlock* blockAt(word id) const { return blocks_[id]; }
  BlockMap* blockMap() const { return block_map_; }

  void setEntryBlock(BasicBlock* block) { entry_block_ = block; }

 private:
  Thread* thread_{nullptr};
  Vector<BasicBlock*> blocks_;
  word next_block_id_{0};
  Vector<Register*> registers_;
  word next_register_id_{0};
  BlockMap* block_map_{nullptr};
  BasicBlock* entry_block_{nullptr};

  DISALLOW_HEAP_ALLOCATION();
  DISALLOW_COPY_AND_ASSIGN(CFG);
};

class IRFunc {
 public:
  IRFunc(HandleScope* scope, Thread* thread, const Function& function, CFG* cfg)
      : thread_(thread), function_(scope, *function), cfg_(cfg) {}
  CFG* cfg() const { return cfg_; }

 private:
  Thread* thread_{nullptr};
  Function function_;
  CFG* cfg_{nullptr};
};

class Instruction {
 public:
  Instruction(Bytecode bc) : bc_(bc) {}
  Instruction(Bytecode bc, const Vector<Register*>& operands)
      : bc_(bc), operands_(operands) {}
  Bytecode opcode() const { return bc_; }
  const char* opname() const { return kBytecodeNames[opcode()]; }

 private:
  Bytecode bc_;
  Vector<Register*> operands_;
};

class Branch : public Instruction {
 public:
  Branch(BasicBlock* target) : Instruction(JUMP_ABSOLUTE), target_(target) {}

  static Branch* cast(Instruction* instr) {
    CHECK(instr->opcode() == JUMP_ABSOLUTE, "Expected Branch but got %s",
          instr->opname());
    return static_cast<Branch*>(instr);
  }

 private:
  BasicBlock* target_;
};

RawObject optimizeBytecode(Thread* thread, const Function& function);

}  // namespace py
