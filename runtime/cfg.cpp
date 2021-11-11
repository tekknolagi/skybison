/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#include "cfg.h"
// TODO(max): Split WordSet out into its own file and use from here.
#include <deque>
#include <set>

#include "heap-profiler.h"

namespace py {

CFG::~CFG() {
  for (word i = 0; i < numBlocks(); i++) {
    delete blockAt(i);
  }

  for (word i = 0; i < registers_.size(); i++) {
    delete registers_[i];
  }
}

BasicBlock* CFG::allocateBlock() {
  BasicBlock* result = new BasicBlock(next_block_id_++);
  blocks_.push_back(result);
  return result;
}

Register* CFG::allocateRegister() {
  Register* result = new Register(next_register_id_++);
  registers_.push_back(result);
  return result;
}

class MakeOrderedVisitor : public WordVisitor {
 public:
  void visit(uword element) { block_starts.push_back(element); }
  word at(word i) const { return block_starts[i]; }
  word size() const { return block_starts.size(); }

 private:
  Vector<word> block_starts;
};

static BlockMap* createBlocks(CFG* cfg,
                              const BytecodeInstructionBlock& bc_block) {
  // Set of instruction indices that begin starts of basic blocks. Add 0 by
  // default because the function starts from the top.
  WordSet block_starts;
  block_starts.add(0);
  const BytecodeView& bc_instrs = bc_block.bytecode();
  word num_instrs = bc_instrs.numInstructions();
  auto maybe_add_next_instr = [&](BytecodeInstruction instr) {
    word next_instr_idx = instr.nextInstrIdx();
    if (next_instr_idx < num_instrs) {
      block_starts.add(next_instr_idx);
    }
  };
  // Mark the beginning of each block in the bytecode
  for (word i = 0; i < num_instrs; i++) {
    BytecodeInstruction instr = bc_instrs.instrAt(i);
    if (instr.isBranch()) {
      maybe_add_next_instr(instr);
      block_starts.add(i);
    } else if (instr.isReturn()) {
      maybe_add_next_instr(instr);
    } else {
      CHECK(!instr.isTerminator(), "Terminator should split block");
    }
  }
  // Expand the block_starts into a list
  MakeOrderedVisitor ordered;
  block_starts.visitElements(&ordered);
  // Allocate basic blocks corresponding to bytecode slices
  BlockMap* block_map = new BlockMap();
  for (word i = 0; i < ordered.size();) {
    word start_idx = ordered.at(i);
    i++;
    word end_idx = i < ordered.size() ? ordered.at(i) : num_instrs;
    BasicBlock* block = cfg->allocateBlock();
    block_map->addBlock(start_idx, end_idx, block, bc_block.bytecode());
  }
  return block_map;
}

// Ensure that the entry block isn't a loop header because we want
// initialization code at the top of the function that doesn't run at every
// loop.
static BasicBlock* getEntryBlock(CFG* cfg, const BytecodeView& bc_instrs) {
  word num_instrs = bc_instrs.numInstructions();
  for (word i = 0; i < num_instrs; i++) {
    BytecodeInstruction instr = bc_instrs.instrAt(i);
    if (instr.isBranch() && instr.jumpTarget() == 0) {
      return cfg->allocateBlock();
    }
  }
  return cfg->blockMap()->blockAtOffset(0);
}

class FrameState {
 public:
  void push(Register* value) { stack_.push(value); }
  Register* pop() { return stack_.pop(); }

 private:
  Stack<Register*> stack_;
  Vector<Register*> locals_;

  DISALLOW_HEAP_ALLOCATION();
  // TODO(max): Move semantics?
  // DISALLOW_COPY_AND_ASSIGN(FrameState);
};

class TranslationContext {
 public:
  TranslationContext(BasicBlock* block, const FrameState& frame)
      : block_(block), frame_(frame) {}
  BasicBlock* block() const { return block_; }
  void setBlock(BasicBlock* block) { block_ = block; }

 private:
  BasicBlock* block_{nullptr};
  FrameState frame_;

  DISALLOW_HEAP_ALLOCATION();
  // TODO(max): Move semantics?
  // DISALLOW_COPY_AND_ASSIGN(TranslationContext);
};

static void translate(IRFunc* irfunc, const TranslationContext& entry_tc) {
  std::deque<TranslationContext> queue({entry_tc});
  std::set<BasicBlock*> processed;
  std::set<BasicBlock*> loop_headers;
  while (!queue.empty()) {
    TranslationContext tc = queue.front();
    queue.pop_front();
    if (processed.count(tc.block())) {
      continue;
    }
    processed.emplace(tc.block());
    // TODO(max): Loop over bytecode
  }
  (void)irfunc;
  std::abort();
}

static RawObject lowerToBytecode(IRFunc* irfunc) {
  (void)irfunc;
  std::abort();
}

RawObject optimizeBytecode(Thread* thread, const Function& function) {
  HandleScope scope(thread);
  CFG cfg;
  IRFunc irfunc(&scope, thread, function, &cfg);
  MutableBytes rewritten_bytecode(&scope, function.rewrittenBytecode());
  BytecodeView bc_view(rewritten_bytecode);
  BytecodeInstructionBlock bc_block = BytecodeInstructionBlock(bc_view);
  BlockMap* block_map = createBlocks(&cfg, bc_block);
  BasicBlock* entry_block = getEntryBlock(&cfg, bc_view);
  cfg.setEntryBlock(entry_block);
  TranslationContext entry_tc(entry_block, FrameState());
  BasicBlock* first_block = block_map->blockAtOffset(0);
  if (entry_block != first_block) {
    entry_block->emit(new Branch(first_block));
  }
  entry_tc.setBlock(first_block);
  translate(&irfunc, entry_tc);
  // TODO(max): Remove trampoline blocks
  // TODO(max): Remove unreachable blocks
  return lowerToBytecode(&irfunc);
}

}  // namespace py
