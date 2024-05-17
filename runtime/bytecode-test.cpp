// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "bytecode.h"

#include "gtest/gtest.h"

#include "ic.h"
#include "test-utils.h"

namespace py {
namespace testing {

using BytecodeTest = RuntimeFixture;

TEST_F(BytecodeTest, NextBytecodeOpReturnsNextBytecodeOpPair) {
  HandleScope scope(thread_);
  const byte bytecode_raw[] = {
      NOP,          99,   0, 0, EXTENDED_ARG, 0xca, 0, 0,
      LOAD_ATTR,    0xfe, 0, 0, LOAD_GLOBAL,  10,   0, 0,
      EXTENDED_ARG, 1,    0, 0, EXTENDED_ARG, 2,    0, 0,
      EXTENDED_ARG, 3,    0, 0, LOAD_ATTR,    4,    0, 0};
  Bytes original_bytecode(&scope, runtime_->newBytesWithAll(bytecode_raw));
  MutableBytes bytecode(
      &scope, runtime_->mutableBytesFromBytes(thread_, original_bytecode));
  word index = 0;
  BytecodeOp bc = nextBytecodeOp(bytecode, &index);
  EXPECT_EQ(bc.bc, NOP);
  EXPECT_EQ(bc.arg, 99);

  bc = nextBytecodeOp(bytecode, &index);
  EXPECT_EQ(bc.bc, LOAD_ATTR);
  EXPECT_EQ(bc.arg, 0xcafe);

  bc = nextBytecodeOp(bytecode, &index);
  EXPECT_EQ(bc.bc, LOAD_GLOBAL);
  EXPECT_EQ(bc.arg, 10);

  bc = nextBytecodeOp(bytecode, &index);
  EXPECT_EQ(bc.bc, LOAD_ATTR);
  EXPECT_EQ(bc.arg, 0x01020304);
}

TEST_F(BytecodeTest, OpargFromObject) {
  EXPECT_EQ(NoneType::object(),
            objectFromOparg(opargFromObject(NoneType::object())));
  EXPECT_EQ(SmallInt::fromWord(-1),
            objectFromOparg(opargFromObject(SmallInt::fromWord(-1))));
  EXPECT_EQ(SmallInt::fromWord(-64),
            objectFromOparg(opargFromObject(SmallInt::fromWord(-64))));
  EXPECT_EQ(SmallInt::fromWord(0),
            objectFromOparg(opargFromObject(SmallInt::fromWord(0))));
  EXPECT_EQ(SmallInt::fromWord(63),
            objectFromOparg(opargFromObject(SmallInt::fromWord(63))));
  EXPECT_EQ(Str::empty(), objectFromOparg(opargFromObject(Str::empty())));
  // Not immediate since it doesn't fit in byte.
  EXPECT_NE(SmallInt::fromWord(64),
            objectFromOparg(opargFromObject(SmallInt::fromWord(64))));
}

TEST_F(BytecodeTest, RewriteBytecodeWithMoreThanCacheLimitCapsRewriting) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  static const int cache_limit = 65536;
  byte bytecode[(cache_limit + 2) * kCompilerCodeUnitSize];
  std::memset(bytecode, 0, sizeof bytecode);
  for (word i = 0; i < cache_limit; i++) {
    bytecode[i * kCompilerCodeUnitSize] = LOAD_ATTR;
    bytecode[(i * kCompilerCodeUnitSize) + 1] = i * 3;
  }
  // LOAD_GLOBAL 1039 == 4 * 256 + 15.
  bytecode[cache_limit * kCompilerCodeUnitSize] = EXTENDED_ARG;
  bytecode[cache_limit * kCompilerCodeUnitSize + 1] = 4;
  bytecode[(cache_limit + 1) * kCompilerCodeUnitSize] = LOAD_GLOBAL;
  bytecode[(cache_limit + 1) * kCompilerCodeUnitSize + 1] = 15;

  word global_names_length = 600;
  Tuple consts(&scope, runtime_->emptyTuple());
  MutableTuple names(&scope, runtime_->newMutableTuple(global_names_length));
  for (word i = 0; i < global_names_length; i++) {
    names.atPut(i, runtime_->newStrFromFmt("g%w", i));
  }
  Tuple names_tuple(&scope, names.becomeImmutable());
  Code code(&scope, newCodeWithBytesConstsNames(bytecode, consts, names_tuple));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));

  // newFunctionWithCode() calls rewriteBytecode().
  Object rewritten_bytecode_obj(&scope, function.rewrittenBytecode());
  ASSERT_TRUE(rewritten_bytecode_obj.isMutableBytes());
  MutableBytes rewritten_bytecode(&scope, *rewritten_bytecode_obj);
  word expected_cache = global_names_length / kIcPointersPerEntry;
  word i = 0;
  for (; i < cache_limit - global_names_length / kIcPointersPerEntry;) {
    BytecodeOp op = nextBytecodeOp(rewritten_bytecode, &i);
    EXPECT_EQ(op.bc, LOAD_ATTR_ANAMORPHIC)
        << "unexpected " << kBytecodeNames[op.bc] << " at idx " << i;
    EXPECT_EQ(op.arg, ((i - 1) * 3) % 256);  // What fits in a byte
    EXPECT_EQ(op.cache, expected_cache);
    expected_cache++;
  }
  for (; i < cache_limit;) {
    BytecodeOp op = nextBytecodeOp(rewritten_bytecode, &i);
    EXPECT_EQ(op.bc, LOAD_ATTR)
        << "unexpected " << kBytecodeNames[op.bc] << " at idx " << i;
  }
  BytecodeOp op = nextBytecodeOp(rewritten_bytecode, &i);
  EXPECT_EQ(op.bc, LOAD_GLOBAL);
  EXPECT_EQ(op.arg, 1039);
  EXPECT_EQ(op.cache, 0);
  EXPECT_EQ(Tuple::cast(function.caches()).length(),
            cache_limit * kIcPointersPerEntry);
  // The cache for LOAD_GLOBAL was populated.
  EXPECT_GT(Tuple::cast(function.caches()).length(), 527);
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesLoadAttrOperations) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      NOP,          99,  EXTENDED_ARG, 0xca, LOAD_ATTR,    0xfe,
      NOP,          106, EXTENDED_ARG, 1,    EXTENDED_ARG, 2,
      EXTENDED_ARG, 3,   LOAD_ATTR,    4,    LOAD_ATTR,    77,
  };
  Code code(&scope, newCodeWithBytes(bytecode));
  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      NOP,
      99,
      0,
      0,
      EXTENDED_ARG,
      0xca,
      0,
      0,
      LOAD_ATTR_ANAMORPHIC,
      0xfe,
      0,
      0,
      NOP,
      106,
      0,
      0,
      EXTENDED_ARG,
      1,
      0,
      0,
      EXTENDED_ARG,
      2,
      0,
      0,
      EXTENDED_ARG,
      3,
      0,
      0,
      LOAD_ATTR_ANAMORPHIC,
      4,
      1,
      0,
      LOAD_ATTR_ANAMORPHIC,
      77,
      2,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));

  ASSERT_TRUE(function.caches().isTuple());
  Tuple caches(&scope, function.caches());
  EXPECT_EQ(caches.length(), 3 * kIcPointersPerEntry);
  for (word i = 0, length = caches.length(); i < length; i++) {
    EXPECT_TRUE(caches.at(i).isNoneType()) << "index " << i;
  }
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesLoadConstOperations) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {LOAD_CONST, 0, LOAD_CONST, 1, LOAD_CONST, 2,
                           LOAD_CONST, 3, LOAD_CONST, 4};

  // Immediate objects.
  Object obj0(&scope, NoneType::object());
  Object obj1(&scope, SmallInt::fromWord(0));
  Object obj2(&scope, Str::empty());
  // Not immediate since it doesn't fit in byte.
  Object obj3(&scope, SmallInt::fromWord(64));
  // Not immediate since it's a heap object.
  Object obj4(&scope, runtime_->newList());
  Tuple consts(&scope,
               runtime_->newTupleWithN(5, &obj0, &obj1, &obj2, &obj3, &obj4));
  Code code(&scope, newCodeWithBytesConsts(bytecode, consts));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));

  byte expected[] = {
      LOAD_IMMEDIATE,
      static_cast<byte>(opargFromObject(NoneType::object())),
      0,
      0,
      LOAD_IMMEDIATE,
      static_cast<byte>(opargFromObject(SmallInt::fromWord(0))),
      0,
      0,
      LOAD_IMMEDIATE,
      static_cast<byte>(opargFromObject(Str::empty())),
      0,
      0,
      LOAD_CONST,
      3,
      0,
      0,
      LOAD_CONST,
      4,
      0,
      0,
  };
  MutableBytes rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesLoadConstToLoadBool) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {LOAD_CONST, 0, LOAD_CONST, 1};

  // Immediate objects.
  Object obj0(&scope, Bool::trueObj());
  Object obj1(&scope, Bool::falseObj());
  Tuple consts(&scope, runtime_->newTupleWith2(obj0, obj1));
  Code code(&scope, newCodeWithBytesConsts(bytecode, consts));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));

  byte expected[] = {
      LOAD_BOOL, 0x80, 0, 0, LOAD_BOOL, 0, 0, 0,
  };
  MutableBytes rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesLoadMethodOperations) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      NOP,          99,  EXTENDED_ARG, 0xca, LOAD_METHOD,  0xfe,
      NOP,          160, EXTENDED_ARG, 1,    EXTENDED_ARG, 2,
      EXTENDED_ARG, 3,   LOAD_METHOD,  4,    LOAD_METHOD,  77,
  };
  Code code(&scope, newCodeWithBytes(bytecode));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      NOP,
      99,
      0,
      0,
      EXTENDED_ARG,
      0xca,
      0,
      0,
      LOAD_METHOD_ANAMORPHIC,
      0xfe,
      0,
      0,
      NOP,
      160,
      0,
      0,
      EXTENDED_ARG,
      1,
      0,
      0,
      EXTENDED_ARG,
      2,
      0,
      0,
      EXTENDED_ARG,
      3,
      0,
      0,
      LOAD_METHOD_ANAMORPHIC,
      4,
      1,
      0,
      LOAD_METHOD_ANAMORPHIC,
      77,
      2,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));

  ASSERT_TRUE(function.caches().isTuple());
  Tuple caches(&scope, function.caches());
  EXPECT_EQ(caches.length(), 3 * kIcPointersPerEntry);
  for (word i = 0, length = caches.length(); i < length; i++) {
    EXPECT_TRUE(caches.at(i).isNoneType()) << "index " << i;
  }
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesStoreAttr) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {STORE_ATTR, 48};
  Code code(&scope, newCodeWithBytes(bytecode));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      STORE_ATTR_ANAMORPHIC,
      48,
      0,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesBinaryOpcodes) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      BINARY_MATRIX_MULTIPLY,
      0,
      BINARY_POWER,
      0,
      BINARY_MULTIPLY,
      0,
      BINARY_MODULO,
      0,
      BINARY_ADD,
      0,
      BINARY_SUBTRACT,
      0,
      BINARY_FLOOR_DIVIDE,
      0,
      BINARY_TRUE_DIVIDE,
      0,
      BINARY_LSHIFT,
      0,
      BINARY_RSHIFT,
      0,
      BINARY_AND,
      0,
      BINARY_XOR,
      0,
      BINARY_OR,
      0,
  };
  Code code(&scope, newCodeWithBytes(bytecode));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MATMUL),
      0,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::POW),
      1,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MUL),
      2,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MOD),
      3,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::ADD),
      4,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::SUB),
      5,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::FLOORDIV),
      6,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::TRUEDIV),
      7,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::LSHIFT),
      8,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::RSHIFT),
      9,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::AND),
      10,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::XOR),
      11,
      0,
      BINARY_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::OR),
      12,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesInplaceOpcodes) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      INPLACE_MATRIX_MULTIPLY,
      0,
      INPLACE_POWER,
      0,
      INPLACE_MULTIPLY,
      0,
      INPLACE_MODULO,
      0,
      INPLACE_ADD,
      0,
      INPLACE_SUBTRACT,
      0,
      INPLACE_FLOOR_DIVIDE,
      0,
      INPLACE_TRUE_DIVIDE,
      0,
      INPLACE_LSHIFT,
      0,
      INPLACE_RSHIFT,
      0,
      INPLACE_AND,
      0,
      INPLACE_XOR,
      0,
      INPLACE_OR,
      0,
  };
  Code code(&scope, newCodeWithBytes(bytecode));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MATMUL),
      0,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::POW),
      1,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MUL),
      2,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::MOD),
      3,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::ADD),
      4,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::SUB),
      5,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::FLOORDIV),
      6,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::TRUEDIV),
      7,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::LSHIFT),
      8,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::RSHIFT),
      9,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::AND),
      10,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::XOR),
      11,
      0,
      INPLACE_OP_ANAMORPHIC,
      static_cast<word>(Interpreter::BinaryOp::OR),
      12,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesCompareOpOpcodes) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      COMPARE_OP, CompareOp::LT,        COMPARE_OP, CompareOp::LE,
      COMPARE_OP, CompareOp::EQ,        COMPARE_OP, CompareOp::NE,
      COMPARE_OP, CompareOp::GT,        COMPARE_OP, CompareOp::GE,
      COMPARE_OP, CompareOp::IN,        COMPARE_OP, CompareOp::NOT_IN,
      COMPARE_OP, CompareOp::IS,        COMPARE_OP, CompareOp::IS_NOT,
      COMPARE_OP, CompareOp::EXC_MATCH,
  };
  Code code(&scope, newCodeWithBytes(bytecode));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      COMPARE_OP_ANAMORPHIC,
      CompareOp::LT,
      0,
      0,
      COMPARE_OP_ANAMORPHIC,
      CompareOp::LE,
      1,
      0,
      COMPARE_OP_ANAMORPHIC,
      CompareOp::EQ,
      2,
      0,
      COMPARE_OP_ANAMORPHIC,
      CompareOp::NE,
      3,
      0,
      COMPARE_OP_ANAMORPHIC,
      CompareOp::GT,
      4,
      0,
      COMPARE_OP_ANAMORPHIC,
      CompareOp::GE,
      5,
      0,
      COMPARE_IN_ANAMORPHIC,
      0,
      6,
      0,
      COMPARE_OP,
      CompareOp::NOT_IN,
      0,
      0,
      COMPARE_IS,
      0,
      0,
      0,
      COMPARE_IS_NOT,
      0,
      0,
      0,
      COMPARE_OP,
      CompareOp::EXC_MATCH,
      0,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesReservesCachesForGlobalVariables) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  const byte bytecode[] = {
      LOAD_GLOBAL, 0, STORE_GLOBAL, 1, LOAD_ATTR, 9, DELETE_GLOBAL, 2,
      STORE_NAME,  3, DELETE_NAME,  4, LOAD_ATTR, 9, LOAD_NAME,     5,
  };
  Tuple consts(&scope, runtime_->emptyTuple());
  MutableTuple names(&scope, runtime_->newMutableTuple(12));
  for (word i = 0; i < 12; i++) {
    names.atPut(i, runtime_->newStrFromFmt("g%w", i));
  }
  Tuple names_tuple(&scope, names.becomeImmutable());
  Code code(&scope, newCodeWithBytesConstsNames(bytecode, consts, names_tuple));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      LOAD_GLOBAL,
      0,
      0,
      0,
      STORE_GLOBAL,
      1,
      0,
      0,
      // Note that LOAD_ATTR's cache index starts at 6 to reserve the first 6
      // cache lines for 12 global variables.
      LOAD_ATTR_ANAMORPHIC,
      9,
      6,
      0,
      DELETE_GLOBAL,
      2,
      0,
      0,
      STORE_NAME,
      3,
      0,
      0,
      DELETE_NAME,
      4,
      0,
      0,
      LOAD_ATTR_ANAMORPHIC,
      9,
      7,
      0,
      LOAD_NAME,
      5,
      0,
      0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));

  Tuple caches(&scope, function.caches());
  word num_global = 6;
  word num_attr = 2;
  EXPECT_EQ(caches.length(), (num_global + num_attr) * kIcPointersPerEntry);
}

TEST_F(
    BytecodeTest,
    RewriteBytecodeDoesNotRewriteLoadFastAndStoreFastOpcodesWithLargeLocalCount) {
  HandleScope scope(thread_);
  Object arg0(&scope, Runtime::internStrFromCStr(thread_, "arg0"));
  Object var0(&scope, Runtime::internStrFromCStr(thread_, "var0"));
  Object var1(&scope, Runtime::internStrFromCStr(thread_, "var1"));
  Tuple varnames(&scope, runtime_->newTupleWith3(arg0, var0, var1));
  Object freevar0(&scope, Runtime::internStrFromCStr(thread_, "freevar0"));
  Tuple freevars(&scope, runtime_->newTupleWith1(freevar0));
  Object cellvar0(&scope, Runtime::internStrFromCStr(thread_, "cellvar0"));
  Tuple cellvars(&scope, runtime_->newTupleWith1(cellvar0));
  word argcount = 1;
  // Set nlocals > 255
  word nlocals = kMaxByte + 3;
  byte bytecode[] = {
      LOAD_FAST,  2, LOAD_FAST,  1, LOAD_FAST,  1,
      STORE_FAST, 2, STORE_FAST, 1, STORE_FAST, 0,
  };
  Bytes code_code(&scope, runtime_->newBytesWithAll(bytecode));
  Object empty_tuple(&scope, runtime_->emptyTuple());
  Object empty_string(&scope, Str::empty());
  Object lnotab(&scope, Bytes::empty());
  word flags = Code::Flags::kOptimized | Code::Flags::kNewlocals;
  Code code(&scope,
            runtime_->newCode(argcount, /*posonlyargcount=*/0,
                              /*kwonlyargcount=*/0, nlocals,
                              /*stacksize=*/0, /*flags=*/flags, code_code,
                              /*consts=*/empty_tuple, /*names=*/empty_tuple,
                              varnames, freevars, cellvars,
                              /*filename=*/empty_string, /*name=*/empty_string,
                              /*firstlineno=*/0, lnotab));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope, runtime_->newFunctionWithCode(thread_, empty_string,
                                                          code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      LOAD_FAST,  2, 0, 0, LOAD_FAST,  1, 0, 0, LOAD_FAST,  1, 0, 0,
      STORE_FAST, 2, 0, 0, STORE_FAST, 1, 0, 0, STORE_FAST, 0, 0, 0,
  };

  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
  EXPECT_TRUE(function.caches().isNoneType());
}

TEST_F(BytecodeTest, RewriteBytecodeRewritesLoadFastAndStoreFastOpcodes) {
  HandleScope scope(thread_);
  Object arg0(&scope, Runtime::internStrFromCStr(thread_, "arg0"));
  Object var0(&scope, Runtime::internStrFromCStr(thread_, "var0"));
  Object var1(&scope, Runtime::internStrFromCStr(thread_, "var1"));
  Tuple varnames(&scope, runtime_->newTupleWith3(arg0, var0, var1));
  Object freevar0(&scope, Runtime::internStrFromCStr(thread_, "freevar0"));
  Tuple freevars(&scope, runtime_->newTupleWith1(freevar0));
  Object cellvar0(&scope, Runtime::internStrFromCStr(thread_, "cellvar0"));
  Tuple cellvars(&scope, runtime_->newTupleWith1(cellvar0));
  word argcount = 1;
  word nlocals = 3;
  byte bytecode[] = {
      LOAD_FAST,  2, LOAD_FAST,  1, LOAD_FAST,  1,
      STORE_FAST, 2, STORE_FAST, 1, STORE_FAST, 0,
  };
  Bytes code_code(&scope, runtime_->newBytesWithAll(bytecode));
  Object empty_tuple(&scope, runtime_->emptyTuple());
  Object empty_string(&scope, Str::empty());
  Object lnotab(&scope, Bytes::empty());
  word flags = Code::Flags::kOptimized | Code::Flags::kNewlocals;
  Code code(&scope,
            runtime_->newCode(argcount, /*posonlyargcount=*/0,
                              /*kwonlyargcount=*/0, nlocals,
                              /*stacksize=*/0, /*flags=*/flags, code_code,
                              /*consts=*/empty_tuple, /*names=*/empty_tuple,
                              varnames, freevars, cellvars,
                              /*filename=*/empty_string, /*name=*/empty_string,
                              /*firstlineno=*/0, lnotab));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope, runtime_->newFunctionWithCode(thread_, empty_string,
                                                          code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      LOAD_FAST_REVERSE,  2, 0, 0, LOAD_FAST_REVERSE,  3, 0, 0,
      LOAD_FAST_REVERSE,  3, 0, 0, STORE_FAST_REVERSE, 2, 0, 0,
      STORE_FAST_REVERSE, 3, 0, 0, STORE_FAST_REVERSE, 4, 0, 0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
  EXPECT_TRUE(function.caches().isNoneType());
}

TEST_F(
    BytecodeTest,
    RewriteBytecodeRewritesLoadFastToLoadFastReverseWhenDeleteFastIsPresent) {
  HandleScope scope(thread_);
  Object arg0(&scope, Runtime::internStrFromCStr(thread_, "arg0"));
  Object var0(&scope, Runtime::internStrFromCStr(thread_, "var0"));
  Object var1(&scope, Runtime::internStrFromCStr(thread_, "var1"));
  Tuple varnames(&scope, runtime_->newTupleWith3(arg0, var0, var1));
  Object freevar0(&scope, Runtime::internStrFromCStr(thread_, "freevar0"));
  Tuple freevars(&scope, runtime_->newTupleWith1(freevar0));
  Object cellvar0(&scope, Runtime::internStrFromCStr(thread_, "cellvar0"));
  Tuple cellvars(&scope, runtime_->newTupleWith1(cellvar0));
  word argcount = 1;
  word nlocals = 3;
  const byte bytecode[] = {
      LOAD_FAST,   2, LOAD_FAST,    1, LOAD_FAST,  0,
      STORE_FAST,  2, STORE_FAST,   1, STORE_FAST, 0,
      DELETE_FAST, 0, RETURN_VALUE, 0,
  };
  Bytes code_code(&scope, runtime_->newBytesWithAll(bytecode));
  Object empty_tuple(&scope, runtime_->emptyTuple());
  Object empty_string(&scope, Str::empty());
  Object lnotab(&scope, Bytes::empty());
  word flags = Code::Flags::kOptimized | Code::Flags::kNewlocals;
  Code code(&scope,
            runtime_->newCode(argcount, /*posonlyargcount=*/0,
                              /*kwonlyargcount=*/0, nlocals,
                              /*stacksize=*/0, /*flags=*/flags, code_code,
                              /*consts=*/empty_tuple, /*names=*/empty_tuple,
                              varnames, freevars, cellvars,
                              /*filename=*/empty_string, /*name=*/empty_string,
                              /*firstlineno=*/0, lnotab));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope, runtime_->newFunctionWithCode(thread_, empty_string,
                                                          code, module));
  // newFunctionWithCode() calls rewriteBytecode().

  byte expected[] = {
      LOAD_FAST_REVERSE,  2, 0, 0, LOAD_FAST_REVERSE,  3, 0, 0,
      LOAD_FAST_REVERSE,  4, 0, 0, STORE_FAST_REVERSE, 2, 0, 0,
      STORE_FAST_REVERSE, 3, 0, 0, STORE_FAST_REVERSE, 4, 0, 0,
      DELETE_FAST,        0, 0, 0, RETURN_VALUE,       0, 0, 0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
  EXPECT_TRUE(function.caches().isNoneType());
}

TEST_F(BytecodeTest,
       RewriteBytecodeDoesNotRewriteFunctionsWithNoOptimizedNorNewLocalsFlag) {
  HandleScope scope(thread_);
  Object name(&scope, Str::empty());
  Tuple consts(&scope, runtime_->emptyTuple());
  Tuple names(&scope, runtime_->emptyTuple());
  const byte bytecode[] = {
      NOP,          99,  EXTENDED_ARG, 0xca, LOAD_ATTR,    0xfe,
      NOP,          106, EXTENDED_ARG, 1,    EXTENDED_ARG, 2,
      EXTENDED_ARG, 3,   LOAD_ATTR,    4,    LOAD_ATTR,    77,
  };
  Code code(&scope,
            newCodeWithBytesConstsNamesFlags(bytecode, consts, names, 0));

  Module module(&scope, findMainModule(runtime_));
  Function function(&scope,
                    runtime_->newFunctionWithCode(thread_, name, code, module));

  byte expected[] = {
      NOP,          99,   0, 0, EXTENDED_ARG, 0xca, 0, 0,
      LOAD_ATTR,    0xfe, 0, 0, NOP,          106,  0, 0,
      EXTENDED_ARG, 1,    0, 0, EXTENDED_ARG, 2,    0, 0,
      EXTENDED_ARG, 3,    0, 0, LOAD_ATTR,    4,    0, 0,
      LOAD_ATTR,    77,   0, 0,
  };
  Object rewritten_bytecode(&scope, function.rewrittenBytecode());
  EXPECT_TRUE(isMutableBytesEqualsBytes(rewritten_bytecode, expected));
}

}  // namespace testing
}  // namespace py
