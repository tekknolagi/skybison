/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */

#include "cfg.h"

#include "test-utils.h"

namespace py {
namespace testing {

using CFGTest = RuntimeFixture;

TEST_F(CFGTest, AllocateBlockAddsBlock) {
  HandleScope scope(thread_);
  Function func(&scope, newEmptyFunction());
  CFG cfg(&scope, thread_, func);
  EXPECT_EQ(cfg.numBlocks(), 0);
  BasicBlock* bb0 = cfg.allocateBlock();
  EXPECT_EQ(cfg.numBlocks(), 1);
  EXPECT_EQ(bb0, cfg.blockAt(0));
  BasicBlock* bb1 = cfg.allocateBlock();
  EXPECT_EQ(cfg.numBlocks(), 2);
  EXPECT_EQ(bb1, cfg.blockAt(1));
}

}  // namespace testing
}  // namespace py
