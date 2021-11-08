/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once

#include "handles-decl.h"
#include "handles.h"
#include "stack.h"
#include "thread.h"

namespace py {

class Register {
 public:
  Register(uword id) : id_(id) {}
  uword id() { return id_; }

 private:
  uword id_;
};

class TranslationContext {
 public:
  TranslationContext(word nlocals) { locals.reserve(nlocals); }
  void push(Register* value) { stack.push(value); }
  Register* pop() { return stack.pop(); }

 private:
  Stack<Register*> stack;
  Vector<Register*> locals;
};

class BasicBlock {
 public:
  BasicBlock(uword id) : id_(id) {}
  uword id() { return id_; }

 private:
  uword id_;
};

class CFG {
 public:
  CFG(HandleScope* scope, Thread* thread, const Function& function);
  ~CFG();
  BasicBlock* allocateBlock();
  Register* allocateRegister();

  word numBlocks() { return blocks_.size(); }
  BasicBlock* blockAt(word id) { return blocks_[id]; }

 private:
  Thread* thread_{nullptr};
  Function function_;
  Vector<BasicBlock*> blocks_;
  word next_block_id_{0};
  Vector<Register*> registers_;
  word next_register_id_{0};
};

}  // namespace py
