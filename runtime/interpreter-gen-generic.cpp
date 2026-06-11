// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "interpreter-gen.h"

#include "asserts.h"
#include "interpreter.h"

// Portable fallback for architectures that lack a hand-written assembly
// interpreter (interpreter-gen-x64.cpp is x86-64 only). We delegate to the
// C++ interpreter and disable the JIT-style function compilation path.

namespace py {

Interpreter* createAsmInterpreter() { return createCppInterpreter(); }

bool canCompileFunction(Thread*, const Function&) { return false; }

void compileFunction(Thread*, const Function&) {
  // Unreachable: callers gate on canCompileFunction(), which is always false
  // on this architecture.
  UNIMPLEMENTED("function compilation is not supported on this architecture");
}

}  // namespace py
