/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#include "cfg.h"

namespace py {

CFG::CFG(HandleScope* scope, Thread* thread, const Function& function)
    : thread_(thread), function_(scope, *function) {}

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

}  // namespace py
