// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "test-utils.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include "attributedict.h"
#include "builtins-module.h"
#include "bytearray-builtins.h"
#include "bytes-builtins.h"
#include "compile-utils.h"
#include "debugging.h"
#include "exception-builtins.h"
#include "frame.h"
#include "handles.h"
#include "ic.h"
#include "int-builtins.h"
#include "module-builtins.h"
#include "modules.h"
#include "os.h"
#include "runtime.h"
#include "set-builtins.h"
#include "str-builtins.h"
#include "sys-module.h"
#include "thread.h"
#include "type-builtins.h"
#include "utils.h"

namespace py {

namespace testing {

static RawObject initializeSysWithDefaults(Thread* thread) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  unique_c_ptr<char> path(OS::executablePath());
  Str executable(&scope, runtime->newStrFromCStr(path.get()));
  List python_path(&scope, runtime->newList());
  MutableTuple data(
      &scope, runtime->newMutableTuple(static_cast<word>(SysFlag::kNumFlags)));
  data.atPut(static_cast<word>(SysFlag::kDebug), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kInspect), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kInteractive), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kOptimize), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kDontWriteBytecode),
             SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kNoUserSite), SmallInt::fromWord(1));
  data.atPut(static_cast<word>(SysFlag::kNoSite), SmallInt::fromWord(1));
  data.atPut(static_cast<word>(SysFlag::kIgnoreEnvironment),
             SmallInt::fromWord(1));
  data.atPut(static_cast<word>(SysFlag::kVerbose), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kBytesWarning), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kQuiet), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kHashRandomization),
             SmallInt::fromWord(1));
  data.atPut(static_cast<word>(SysFlag::kIsolated), SmallInt::fromWord(0));
  data.atPut(static_cast<word>(SysFlag::kDevMode), Bool::falseObj());
  data.atPut(static_cast<word>(SysFlag::kUTF8Mode), SmallInt::fromWord(1));
  static_assert(static_cast<word>(SysFlag::kNumFlags) == 15,
                "unexpected flag count");
  Tuple flags_data(&scope, data.becomeImmutable());
  List warnoptions(&scope, runtime->newList());
  return initializeSys(thread, executable, python_path, flags_data, warnoptions,
                       /*extend_python_path_with_stdlib=*/true);
}

bool useCppInterpreter() {
  const char* pyro_cpp_interpreter = std::getenv("PYRO_CPP_INTERPRETER");
  return pyro_cpp_interpreter != nullptr &&
         ::strcmp(pyro_cpp_interpreter, "1") == 0;
}

Runtime* createTestRuntime() {
  bool use_cpp_interpreter = useCppInterpreter();
  word heap_size = 128 * kMiB;
  Interpreter* interpreter =
      use_cpp_interpreter ? createCppInterpreter() : createAsmInterpreter();
  RandomState random_state = randomState();
  Runtime* runtime =
      new Runtime(heap_size, interpreter, random_state, StdioState::kBuffered);
  Thread* thread = Thread::current();
  CHECK(initializeSysWithDefaults(thread).isNoneType(),
        "initializeSys() failed");
  CHECK(runtime->initialize(thread).isNoneType(),
        "Runtime::initialize() failed");
  return runtime;
}

Value::Type Value::type() const { return type_; }

bool Value::boolVal() const {
  DCHECK(type() == Type::Bool, "expected bool");
  return bool_;
}

word Value::intVal() const {
  DCHECK(type() == Type::Int, "expected int");
  return int_;
}

double Value::floatVal() const {
  DCHECK(type() == Type::Float, "expected float");
  return float_;
}

const char* Value::strVal() const {
  DCHECK(type() == Type::Str, "expected str");
  return str_;
}

template <typename T1, typename T2>
::testing::AssertionResult badListValue(const char* actual_expr, word i,
                                        const T1& actual, const T2& expected) {
  return ::testing::AssertionFailure()
         << "Value of: " << actual_expr << '[' << i << "]\n"
         << "  Actual: " << actual << '\n'
         << "Expected: " << expected;
}

::testing::AssertionResult AssertPyListEqual(
    const char* actual_expr, const char* /* expected_expr */,
    const Object& actual, const std::vector<Value>& expected) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();

  if (!actual.isList()) {
    return ::testing::AssertionFailure()
           << " Type of: " << actual_expr << "\n"
           << "  Actual: " << typeName(runtime, *actual) << "\n"
           << "Expected: list";
  }

  HandleScope scope(thread);
  List list(&scope, *actual);
  if (static_cast<size_t>(list.numItems()) != expected.size()) {
    return ::testing::AssertionFailure()
           << "Length of: " << actual_expr << "\n"
           << "   Actual: " << list.numItems() << "\n"
           << " Expected: " << expected.size();
  }

  for (size_t i = 0; i < expected.size(); i++) {
    Object actual_item(&scope, list.at(i));
    const Value& expected_item = expected[i];

    auto bad_type = [&](const char* expected_type) {
      return ::testing::AssertionFailure()
             << " Type of: " << actual_expr << '[' << i << "]\n"
             << "  Actual: " << typeName(runtime, *actual_item) << '\n'
             << "Expected: " << expected_type;
    };

    switch (expected_item.type()) {
      case Value::Type::None: {
        if (!actual_item.isNoneType()) return bad_type("RawNoneType");
        break;
      }

      case Value::Type::Bool: {
        if (!actual_item.isBool()) return bad_type("bool");
        auto const actual_val = Bool::cast(*actual_item) == Bool::trueObj();
        auto const expected_val = expected_item.boolVal();
        if (actual_val != expected_val) {
          return badListValue(actual_expr, i, actual_val ? "True" : "False",
                              expected_val ? "True" : "False");
        }
        break;
      }

      case Value::Type::Int: {
        if (!actual_item.isInt()) return bad_type("int");
        Int actual_val(&scope, *actual_item);
        Int expected_val(&scope, runtime->newInt(expected_item.intVal()));
        if (actual_val.compare(*expected_val) != 0) {
          // TODO(bsimmers): Support multi-digit values when we can print them.
          return badListValue(actual_expr, i, actual_val.digitAt(0),
                              expected_item.intVal());
        }
        break;
      }

      case Value::Type::Float: {
        if (!actual_item.isFloat()) return bad_type("float");
        auto const actual_val = Float::cast(*actual_item).value();
        auto const expected_val = expected_item.floatVal();
        if (std::abs(actual_val - expected_val) >= DBL_EPSILON) {
          return badListValue(actual_expr, i, actual_val, expected_val);
        }
        break;
      }

      case Value::Type::Str: {
        if (!actual_item.isStr()) return bad_type("str");
        Str actual_val(&scope, *actual_item);
        const char* expected_val = expected_item.strVal();
        if (!actual_val.equalsCStr(expected_val)) {
          return badListValue(actual_expr, i, actual_val, expected_val);
        }
        break;
      }
    }
  }

  return ::testing::AssertionSuccess();
}

RawObject callFunction(const Function& func, const Tuple& args) {
  Thread* thread = Thread::current();
  thread->stackPush(*func);
  word args_length = args.length();
  for (word i = 0; i < args_length; i++) {
    thread->stackPush(args.at(i));
  }
  return Interpreter::call(thread, args_length);
}

bool tupleContains(const Tuple& object_array, const Object& key) {
  for (word i = 0; i < object_array.length(); i++) {
    if (object_array.at(i) == *key) {
      return true;
    }
  }
  return false;
}

bool listContains(const Object& list_obj, const Object& key) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  if (!thread->runtime()->isInstanceOfList(*list_obj)) {
    return false;
  }
  List list(&scope, *list_obj);
  for (word i = 0, num_items = list.numItems(); i < num_items; i++) {
    if (list.at(i) == *key) {
      return true;
    }
  }
  return false;
}

bool setIncludes(Thread* thread, const SetBase& set, const Object& key) {
  HandleScope scope(thread);
  Object hash_obj(&scope, Interpreter::hash(thread, key));
  CHECK(hash_obj.isSmallInt(), "key must be hashable");
  word hash = SmallInt::cast(*hash_obj).value();
  return setIncludes(thread, set, key, hash);
}

void setHashAndAdd(Thread* thread, const SetBase& set, const Object& value) {
  HandleScope scope(thread);
  Object hash_obj(&scope, Interpreter::hash(thread, value));
  CHECK(hash_obj.isSmallInt(), "value must be hashable");
  word hash = SmallInt::cast(*hash_obj).value();
  setAdd(thread, set, value, hash);
}

static RawObject findModuleByCStr(Runtime* runtime, const char* name) {
  HandleScope scope(Thread::current());
  Object key(&scope, runtime->newStrFromCStr(name));
  return runtime->findModule(key);
}

RawObject findMainModule(Runtime* runtime) {
  return findModuleByCStr(runtime, "__main__");
}

RawObject mainModuleAt(Runtime* runtime, const char* name) {
  return moduleAtByCStr(runtime, "__main__", name);
}

RawObject moduleAtByCStr(Runtime* runtime, const char* module_name,
                         const char* name) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object mod_obj(&scope, findModuleByCStr(runtime, module_name));
  if (mod_obj.isNoneType()) {
    return Error::notFound();
  }
  Module module(&scope, *mod_obj);
  Object name_obj(&scope, Runtime::internStrFromCStr(thread, name));
  return moduleAt(module, name_obj);
}

std::string typeName(Runtime* runtime, RawObject obj) {
  if (obj.layoutId() == LayoutId::kError) return "Error";
  RawStr name = Str::cast(Type::cast(runtime->typeOf(obj)).name());
  word length = name.length();
  std::string result(length, '\0');
  name.copyTo(reinterpret_cast<byte*>(&result[0]), length);
  return result;
}

RawObject typeValueCellAt(RawType type, RawObject name) {
  RawObject result = NoneType::object();
  if (!attributeValueCellAt(type, name, &result)) return Error::notFound();
  return result;
}

static RawCode newCodeHelper(Thread* thread, View<byte> bytes,
                             const Tuple& consts, const Tuple& names,
                             Locals* locals, word flags) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  word argcount = 0;
  word posonlyargcount = 0;
  word kwonlyargcount = 0;
  word nlocals = 0;
  word stacksize = 20;
  Tuple varnames_tuple(&scope, runtime->emptyTuple());
  if (locals != nullptr) {
    argcount = locals->argcount;
    posonlyargcount = locals->posonlyargcount;
    kwonlyargcount = locals->kwonlyargcount;
    nlocals = argcount + kwonlyargcount + locals->varcount;
    if (locals->varargs) {
      nlocals++;
      flags |= Code::Flags::kVarargs;
    }
    if (locals->varkeyargs) {
      nlocals++;
      flags |= Code::Flags::kVarkeyargs;
    }
    MutableTuple varnames(&scope, runtime->newMutableTuple(nlocals));
    word idx = 0;
    for (word i = 0; i < locals->argcount; i++) {
      varnames.atPut(idx++, runtime->newStrFromFmt("arg%w", i));
    }
    if (locals->varargs) {
      varnames.atPut(idx++, runtime->newStrFromCStr("args"));
    }
    if (locals->varkeyargs) {
      varnames.atPut(idx++, runtime->newStrFromCStr("kwargs"));
    }
    for (word i = 0; i < locals->varcount; i++) {
      varnames.atPut(idx++, runtime->newStrFromFmt("var%w", i));
    }
    CHECK(idx == nlocals, "local count mismatch");
    varnames_tuple = varnames.becomeImmutable();
  }

  Bytes code(&scope, runtime->newBytesWithAll(bytes));
  Tuple empty_tuple(&scope, runtime->emptyTuple());
  Object empty_string(&scope, Str::empty());
  Object empty_bytes(&scope, Bytes::empty());
  return Code::cast(runtime->newCode(/*argcount=*/argcount,
                                     /*posonlyargcount=*/posonlyargcount,
                                     /*kwonlyargcount=*/kwonlyargcount,
                                     /*nlocals=*/nlocals,
                                     /*stacksize=*/stacksize,
                                     /*flags=*/flags, code, consts, names,
                                     varnames_tuple,
                                     /*freevars=*/empty_tuple,
                                     /*cellvars=*/empty_tuple,
                                     /*filename=*/empty_string,
                                     /*name=*/empty_string,
                                     /*firstlineno=*/0,
                                     /*lnotab=*/empty_bytes));
}

RawCode newCodeWithBytesConstsNames(View<byte> bytes, const Tuple& consts,
                                    const Tuple& names) {
  Thread* thread = Thread::current();
  word flags = Code::Flags::kOptimized | Code::Flags::kNewlocals;
  return newCodeHelper(thread, bytes, consts, names, /*locals=*/nullptr, flags);
}

RawCode newCodeWithBytesConstsNamesFlags(View<byte> bytes, const Tuple& consts,
                                         const Tuple& names, word flags) {
  Thread* thread = Thread::current();
  return newCodeHelper(thread, bytes, consts, names, /*locals=*/nullptr, flags);
}

RawCode newCodeWithBytesConstsNamesLocals(View<byte> bytes, const Tuple& consts,
                                          const Tuple& names, Locals* locals) {
  Thread* thread = Thread::current();
  word flags = Code::Flags::kOptimized | Code::Flags::kNewlocals;
  return newCodeHelper(thread, bytes, consts, names, locals, flags);
}

RawCode newCodeWithBytesConsts(View<byte> bytes, const Tuple& consts) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Tuple names(&scope, thread->runtime()->emptyTuple());
  return newCodeWithBytesConstsNames(bytes, consts, names);
}

RawCode newCodeWithBytes(View<byte> bytes) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Tuple consts(&scope, thread->runtime()->emptyTuple());
  return newCodeWithBytesConsts(bytes, consts);
}

RawFunction newEmptyFunction() {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Code code(&scope, newCodeWithBytes(View<byte>(nullptr, 0)));
  Object qualname(&scope, Str::empty());
  Module main(&scope, findMainModule(runtime));
  return Function::cast(
      runtime->newFunctionWithCode(thread, qualname, code, main));
}

RawBytes newBytesFromCStr(Thread* thread, const char* str) {
  return Bytes::cast(thread->runtime()->newBytesWithAll(
      View<byte>(reinterpret_cast<const byte*>(str), std::strlen(str))));
}

RawBytearray newBytearrayFromCStr(Thread* thread, const char* str) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Bytearray result(&scope, runtime->newBytearray());
  runtime->bytearrayExtend(
      thread, result,
      View<byte>(reinterpret_cast<const byte*>(str), std::strlen(str)));
  return *result;
}

RawLargeInt newLargeIntWithDigits(View<uword> digits) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  LargeInt result(&scope, thread->runtime()->createLargeInt(digits.length()));
  for (word i = 0, e = digits.length(); i < e; ++i) {
    result.digitAtPut(i, digits.get(i));
  }
  return *result;
}

RawObject newMemoryView(View<byte> bytes, const char* format,
                        ReadOnly read_only) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Bytes bytes_obj(&scope, runtime->newBytesWithAll(bytes));
  if (read_only == ReadOnly::ReadWrite) {
    bytes_obj = runtime->mutableBytesFromBytes(thread, bytes_obj);
  }
  MemoryView result(&scope,
                    runtime->newMemoryView(thread, bytes_obj, bytes_obj,
                                           bytes_obj.length(), read_only));
  result.setFormat(Str::cast(runtime->newStrFromCStr(format)));
  return *result;
}

RawTuple newTupleWithNone(word length) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  MutableTuple tuple(&scope, thread->runtime()->newMutableTuple(length));
  tuple.fill(NoneType::object());
  return Tuple::cast(tuple.becomeImmutable());
}

RawObject newWeakRefWithCallback(Runtime* runtime, Thread* thread,
                                 const Object& referent,
                                 const Object& callback) {
  HandleScope scope(thread);
  WeakRef ref(&scope, runtime->newWeakRef(thread, referent));
  ref.setCallback(runtime->newBoundMethod(callback, ref));
  return *ref;
}

RawObject setFromRange(word start, word stop) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Set result(&scope, thread->runtime()->newSet());
  Object value(&scope, NoneType::object());
  Object hash_obj(&scope, NoneType::object());
  for (word i = start; i < stop; i++) {
    value = SmallInt::fromWord(i);
    hash_obj = Interpreter::hash(thread, value);
    if (hash_obj.isErrorException()) return *hash_obj;
    word hash = SmallInt::cast(*hash_obj).value();
    setAdd(thread, result, value, hash);
  }
  return *result;
}

RawObject runBuiltinImpl(BuiltinFunction function,
                         View<std::reference_wrapper<const Object>> args) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  // Push an empty function so we have one at the expected place in the stack.
  word args_length = args.length();
  Runtime* runtime = thread->runtime();
  Tuple parameter_names(&scope, runtime->emptyTuple());
  if (args_length > 0) {
    MutableTuple parameter_names_mut(&scope,
                                     runtime->newMutableTuple(args_length));
    for (word i = 0; i < args_length; i++) {
      parameter_names_mut.atPut(i, runtime->newStrFromFmt("arg%w", i));
    }
    parameter_names = parameter_names_mut.becomeImmutable();
  }

  Object name(&scope, Runtime::internStrFromCStr(thread, "<anonymous>"));
  Code code(&scope, runtime->newBuiltinCode(args_length, /*posonlyargcount=*/0,
                                            /*kwonlyargcount=*/0,
                                            /*flags=*/0, function,
                                            parameter_names, name));
  Module main(&scope, findMainModule(runtime));
  Function function_obj(&scope,
                        runtime->newFunctionWithCode(thread, name, code, main));

  thread->stackPush(*function_obj);
  for (word i = 0; i < args_length; i++) {
    thread->stackPush(*args.get(i).get());
  }
  return Interpreter::call(thread, args_length);
}

RawObject runBuiltin(BuiltinFunction function) {
  return runBuiltinImpl(function,
                        View<std::reference_wrapper<const Object>>{nullptr, 0});
}

RawObject runCode(const Code& code) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Module main(&scope, findMainModule(runtime));
  Object qualname(&scope, Runtime::internStrFromCStr(thread, "<anonymous>"));
  Function function(&scope,
                    runtime->newFunctionWithCode(thread, qualname, code, main));
  return Interpreter::call0(thread, function);
}

RawObject runCodeNoBytecodeRewriting(const Code& code) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Module main(&scope, findMainModule(runtime));
  Object qualname(&scope, Runtime::internStrFromCStr(thread, "<anonymous>"));
  Bytes bytecode(&scope, code.code());
  code.setCode(runtime->newBytes(0, 0));

  Function function(&scope,
                    runtime->newFunctionWithCode(thread, qualname, code, main));
  MutableBytes rewritten_bytecode(
      &scope, runtime->newMutableBytesUninitialized(bytecode.length()));
  rewritten_bytecode.replaceFromWithBytes(0, *bytecode, bytecode.length());
  function.setRewrittenBytecode(*rewritten_bytecode);
  return Interpreter::call0(thread, function);
}

RawObject runFromCStr(Runtime* runtime, const char* c_str) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str(&scope, runtime->newStrFromCStr(c_str));
  Object filename(&scope, runtime->newStrFromCStr("<test string>"));
  Code code(&scope, compile(thread, str, filename, ID(exec), /*flags=*/0,
                            /*optimize=*/0));
  Module main_module(&scope, findMainModule(runtime));
  Object result(&scope, executeModule(thread, code, main_module));

  // Barebones emulation of the top-level SystemExit handling, to allow for
  // testing of handleSystemExit().
  DCHECK(thread->isErrorValueOk(*result), "error/exception mismatch");
  if (result.isError()) {
    Type type(&scope, thread->pendingExceptionType());
    if (type.builtinBase() == LayoutId::kSystemExit) handleSystemExit(thread);
  }
  return *result;
}

void addBuiltin(const char* name_cstr, BuiltinFunction function,
                View<const char*> parameter_names, word flags) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Module main(&scope, findMainModule(runtime));
  word num_parameters = parameter_names.length();
  Object parameter_names_tuple(&scope, NoneType::object());
  if (num_parameters > 0) {
    MutableTuple parameter_names_mtuple(
        &scope, runtime->newMutableTuple(num_parameters));
    for (word i = 0; i < num_parameters; i++) {
      parameter_names_mtuple.atPut(
          i, Runtime::internStrFromCStr(thread, parameter_names.get(i)));
    }
    parameter_names_tuple = parameter_names_mtuple.becomeImmutable();
  } else {
    parameter_names_tuple = runtime->emptyTuple();
  }
  Object name(&scope, Runtime::internStrFromCStr(thread, name_cstr));
  word argcount = num_parameters - ((flags & Code::Flags::kVarargs) != 0) -
                  ((flags & Code::Flags::kVarkeyargs) != 0);
  Code code(&scope, runtime->newBuiltinCode(
                        /*argcount=*/argcount, /*posonlyargcount=*/0,
                        /*kwonlyargcount=*/0, flags, function,
                        /*parameter_names=*/parameter_names_tuple, name));
  Function function_obj(&scope,
                        runtime->newFunctionWithCode(thread, name, code, main));
  moduleAtPut(thread, main, name, function_obj);
}

// Equivalent to evaluating "list(range(start, stop))" in Python
RawObject listFromRange(word start, word stop) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  List result(&scope, thread->runtime()->newList());
  Object value(&scope, NoneType::object());
  for (word i = start; i < stop; i++) {
    value = SmallInt::fromWord(i);
    thread->runtime()->listAdd(thread, result, value);
  }
  return *result;
}

RawObject icLookupAttr(RawMutableTuple caches, word index, LayoutId layout_id) {
  word i = index * kIcPointersPerEntry;
  bool is_found = false;
  if (caches.at(i + kIcEntryValueOffset).isTuple()) {
    return icLookupPolymorphic(caches, index, layout_id, &is_found);
  }
  return icLookupMonomorphic(caches, index, layout_id, &is_found);
}

RawObject icLookupBinaryOp(RawMutableTuple caches, word index,
                           LayoutId left_layout_id, LayoutId right_layout_id,
                           BinaryOpFlags* flags_out) {
  word i = index * kIcPointersPerEntry;
  if (caches.at(i + kIcEntryValueOffset).isTuple()) {
    return icLookupBinOpPolymorphic(caches, index, left_layout_id,
                                    right_layout_id, flags_out);
  }
  return icLookupBinOpMonomorphic(caches, index, left_layout_id,
                                  right_layout_id, flags_out);
}

::testing::AssertionResult containsBytecode(const Function& function,
                                            Bytecode bc) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  for (word i = 0, num_opcodes = rewrittenBytecodeLength(bytecode);
       i < num_opcodes;) {
    BytecodeOp bco = nextBytecodeOp(bytecode, &i);
    if (bco.bc == bc) {
      return ::testing::AssertionSuccess();
    }
  }
  unique_c_ptr<char> name(Str::cast(function.name()).toCStr());
  return ::testing::AssertionFailure()
         << "opcode " << kBytecodeNames[static_cast<word>(bc)]
         << " not found in '" << name.get() << "'";
}

::testing::AssertionResult isBytearrayEqualsBytes(const Object& result,
                                                  View<byte> expected) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (result.isError()) {
    if (result.isErrorException()) {
      Type type(&scope, thread->pendingExceptionType());
      unique_c_ptr<char> name(Str::cast(type.name()).toCStr());
      return ::testing::AssertionFailure()
             << "pending '" << name.get() << "' exception";
    }
    return ::testing::AssertionFailure() << "is an " << result;
  }
  if (!runtime->isInstanceOfBytearray(*result)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, *result) << "'";
  }
  Bytearray result_array(&scope, *result);
  Bytes result_bytes(&scope, bytearrayAsBytes(thread, result_array));
  Bytes expected_bytes(&scope, runtime->newBytesWithAll(expected));
  if (result_bytes.compare(*expected_bytes) != 0) {
    return ::testing::AssertionFailure()
           << "bytearray(" << result_bytes << ") is not equal to bytearray("
           << expected_bytes << ")";
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isBytearrayEqualsCStr(const Object& result,
                                                 const char* expected) {
  return isBytearrayEqualsBytes(
      result, View<byte>(reinterpret_cast<const byte*>(expected),
                         static_cast<word>(std::strlen(expected))));
}

::testing::AssertionResult isBytesEqualsBytes(const Object& result,
                                              View<byte> expected) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (result.isError()) {
    if (result.isErrorException()) {
      Type type(&scope, thread->pendingExceptionType());
      unique_c_ptr<char> name(Str::cast(type.name()).toCStr());
      return ::testing::AssertionFailure()
             << "pending '" << name.get() << "' exception";
    }
    return ::testing::AssertionFailure() << "is an " << result;
  }
  if (!runtime->isInstanceOfBytes(*result)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, *result) << "'";
  }
  Bytes result_bytes(&scope, bytesUnderlying(*result));
  Bytes expected_bytes(&scope, runtime->newBytesWithAll(expected));
  if (result_bytes.compare(*expected_bytes) != 0) {
    return ::testing::AssertionFailure()
           << result_bytes << " is not equal to " << expected_bytes;
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isMutableBytesEqualsBytes(const Object& result,
                                                     View<byte> expected) {
  if (!result.isError() && !result.isMutableBytes()) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(Thread::current()->runtime(), *result)
           << "'";
  }
  return isBytesEqualsBytes(result, expected);
}

::testing::AssertionResult isBytesEqualsCStr(const Object& result,
                                             const char* expected) {
  return isBytesEqualsBytes(
      result, View<byte>(reinterpret_cast<const byte*>(expected),
                         static_cast<word>(std::strlen(expected))));
}

::testing::AssertionResult isStrEquals(const Object& str1, const Object& str2) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*str1)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, *str1) << "'";
  }
  if (!runtime->isInstanceOfStr(*str2)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, *str2) << "'";
  }
  Str s1(&scope, strUnderlying(*str1));
  Str s2(&scope, strUnderlying(*str2));
  if (!s1.equals(*s2)) {
    unique_c_ptr<char> s2_ptr(s2.toCStr());
    return ::testing::AssertionFailure()
           << "is not equal to '" << s2_ptr.get() << "'";
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isStrEqualsCStr(RawObject obj, const char* c_str) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, obj);
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*str_obj)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, *str_obj) << "'";
  }
  Str str(&scope, strUnderlying(*str_obj));
  if (!str.equalsCStr(c_str)) {
    unique_c_ptr<char> str_ptr(str.toCStr());
    return ::testing::AssertionFailure()
           << "'" << str_ptr.get() << "' is not equal to '" << c_str << "'";
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isSymbolIdEquals(SymbolId result,
                                            SymbolId expected) {
  if (result == expected) return ::testing::AssertionSuccess();
  const char* result_name = result == SymbolId::kInvalid
                                ? "<Invalid>"
                                : Symbols::predefinedSymbolAt(result);
  return ::testing::AssertionFailure()
         << "Expected '" << Symbols::predefinedSymbolAt(expected)
         << "', but got '" << result_name << "'";
}

::testing::AssertionResult isFloatEqualsDouble(RawObject obj, double expected) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (obj.isError()) {
    if (obj.isErrorException()) {
      Type type(&scope, thread->pendingExceptionType());
      Str type_name(&scope, type.name());
      return ::testing::AssertionFailure()
             << "pending " << type_name << " exception";
    }
    return ::testing::AssertionFailure() << "is an " << obj;
  }
  if (!runtime->isInstanceOfFloat(obj)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, obj) << "'";
  }
  double value = floatUnderlying(obj).value();
  // Test with memcmp instead of ==, so we can differentiate -0, and 0 or test
  // for NaNs.
  if (value != expected) {
    return ::testing::AssertionFailure() << value << " is not " << expected;
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isIntEqualsWord(RawObject obj, word value) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (obj.isError()) {
    if (obj.isErrorException()) {
      Type type(&scope, thread->pendingExceptionType());
      Str type_name(&scope, type.name());
      return ::testing::AssertionFailure()
             << "pending " << type_name << " exception";
    }
    return ::testing::AssertionFailure() << "is an " << obj;
  }
  if (!runtime->isInstanceOfInt(obj)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, obj) << "'";
  }
  Object object(&scope, obj);
  Int value_int(&scope, intUnderlying(*object));
  if (value_int.numDigits() > 1 || value_int.asWord() != value) {
    return ::testing::AssertionFailure()
           << value_int << " is not equal to " << value;
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isIntEqualsDigits(RawObject obj,
                                             View<uword> digits) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  if (obj.isError()) {
    if (obj.isErrorException()) {
      Type type(&scope, thread->pendingExceptionType());
      Str type_name(&scope, type.name());
      return ::testing::AssertionFailure()
             << "pending " << type_name << " exception";
    }
    return ::testing::AssertionFailure() << "is an " << obj;
  }
  if (!runtime->isInstanceOfInt(obj)) {
    return ::testing::AssertionFailure()
           << "is a '" << typeName(runtime, obj) << "'";
  }
  Int expected(&scope, newLargeIntWithDigits(digits));
  Object value_obj(&scope, obj);
  Int value_int(&scope, intUnderlying(*value_obj));
  if (expected.compare(*value_int) != 0) {
    return ::testing::AssertionFailure()
           << value_int << " is not equal to " << expected;
  }
  return ::testing::AssertionSuccess();
}

RawObject layoutCreateEmpty(Thread* thread) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  LayoutId id = runtime->reserveLayoutId(thread);
  Layout result(&scope, runtime->newLayout(id));
  runtime->layoutSetTupleOverflow(*result);
  runtime->layoutAtPut(id, *result);
  return *result;
}

::testing::AssertionResult raised(RawObject return_value, LayoutId layout_id) {
  return raisedWithStr(return_value, layout_id, nullptr);
}

::testing::AssertionResult raisedWithStr(RawObject return_value,
                                         LayoutId layout_id,
                                         const char* message) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object return_value_obj(&scope, return_value);

  if (!return_value_obj.isError()) {
    Type type(&scope, runtime->typeOf(*return_value_obj));
    Str name(&scope, type.name());
    return ::testing::AssertionFailure()
           << "call returned " << name << ", not Error";
  }

  if (!thread->hasPendingException()) {
    return ::testing::AssertionFailure() << "no exception pending";
  }

  Type expected_type(&scope, runtime->typeAt(layout_id));
  Type exception_type(&scope, thread->pendingExceptionType());
  if (!typeIsSubclass(*exception_type, *expected_type)) {
    Str expected_name(&scope, expected_type.name());
    Str actual_name(&scope, exception_type.name());
    return ::testing::AssertionFailure()
           << "\npending exception has type:\n  " << actual_name
           << "\nexpected:\n  " << expected_name << "\n";
  }

  if (message == nullptr) return ::testing::AssertionSuccess();

  Object exc_value(&scope, thread->pendingExceptionValue());
  if (!runtime->isInstanceOfStr(*exc_value)) {
    if (runtime->isInstanceOfBaseException(*exc_value)) {
      BaseException exc(&scope, *exc_value);
      Tuple args(&scope, exc.args());
      if (args.length() == 0) {
        return ::testing::AssertionFailure()
               << "pending exception args tuple is empty";
      }
      exc_value = args.at(0);
    }

    if (!runtime->isInstanceOfStr(*exc_value)) {
      return ::testing::AssertionFailure()
             << "pending exception value is not str";
    }
  }

  Str exc_msg(&scope, *exc_value);
  if (!exc_msg.equalsCStr(message)) {
    return ::testing::AssertionFailure()
           << "\npending exception value:\n  '" << exc_msg
           << "'\nexpected:\n  '" << message << "'\n";
  }

  return ::testing::AssertionSuccess();
}

TemporaryDirectory::TemporaryDirectory() {
  const char* tmpdir = std::getenv("TMPDIR");
  if (tmpdir == nullptr) {
    tmpdir = "/tmp";
  }
  const char format[] = "%s/PyroTest.XXXXXXXX";
  word length = std::snprintf(nullptr, 0, format, tmpdir) + 1;
  std::unique_ptr<char[]> buffer(new char[length]);
  std::snprintf(buffer.get(), length, format, tmpdir);
  char* result = ::mkdtemp(buffer.get());
  CHECK(result != nullptr, "failed to create temporary directory");
  path = buffer.get();
  CHECK(!path.empty(), "must not be empty");
  if (path.back() != '/') path += "/";
}

TemporaryDirectory::~TemporaryDirectory() {
  std::string cleanup = "rm -rf " + path;
  CHECK(system(cleanup.c_str()) == 0, "failed to cleanup temporary directory");
}

void writeFile(const std::string& path, const std::string& contents) {
  CHECK(!path.empty() && path.front() == '/', "Should be an absolute path");
  std::ofstream of(path);
  CHECK(of.good(), "file creation failed");
  of << contents;
  CHECK(of.good(), "file write failed");
  of.close();
}

}  // namespace testing
}  // namespace py
