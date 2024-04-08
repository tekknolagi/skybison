// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <sstream>

#include "attributedict.h"
#include "builtins.h"
#include "bytearray-builtins.h"
#include "bytes-builtins.h"
#include "byteslike.h"
#include "capi.h"
#include "debugging.h"
#include "dict-builtins.h"
#include "exception-builtins.h"
#include "file.h"
#include "float-builtins.h"
#include "float-conversion.h"
#include "heap-profiler.h"
#include "int-builtins.h"
#include "list-builtins.h"
#include "memoryview-builtins.h"
#include "module-builtins.h"
#include "modules.h"
#include "mro.h"
#include "object-builtins.h"
#include "range-builtins.h"
#include "set-builtins.h"
#include "slice-builtins.h"
#include "str-builtins.h"
#include "strarray-builtins.h"
#include "structseq-builtins.h"
#include "super-builtins.h"
#include "traceback-builtins.h"
#include "tuple-builtins.h"
#include "type-builtins.h"
#include "unicode.h"
#include "vector.h"

namespace py {

static RawObject raiseRequiresFromCaller(Thread* thread, Arguments args,
                                         SymbolId expected_type) {
  HandleScope scope(thread);
  Function function(&scope,
                    thread->currentFrame()->previousFrame()->function());
  Str function_name(&scope, function.name());
  Object obj(&scope, args.get(0));
  return thread->raiseWithFmt(
      LayoutId::kTypeError,
      "'%S' for '%Y' objects doesn't apply to a '%T' object", &function_name,
      expected_type, &obj);
}

bool FUNC(_builtins, _bool_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isBool()));
  return true;
}

bool FUNC(_builtins, _bool_guard_intrinsic)(Thread* thread) {
  if (thread->stackTop().isBool()) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _bytearray_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(
      thread->runtime()->isInstanceOfBytearray(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _bytearray_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfBytearray(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _bytearray_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPop();
  if (arg.isBytearray()) {
    thread->stackSetTop(SmallInt::fromWord(Bytearray::cast(arg).numItems()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _bytes_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfBytes(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _bytes_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfBytes(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _bytes_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isBytes()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(Bytes::cast(arg).length()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _byteslike_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isByteslike(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _byteslike_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isByteslike(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _complex_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(
      thread->runtime()->isInstanceOfComplex(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _deque_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfDeque(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _dict_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfDict(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _dict_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isDict()));
  return true;
}

bool FUNC(_builtins, _dict_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfDict(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _dict_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isDict()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(Dict::cast(arg).numItems()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _float_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfFloat(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _float_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isFloat()));
  return true;
}

bool FUNC(_builtins, _float_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfFloat(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _frozenset_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(
      thread->runtime()->isInstanceOfFrozenSet(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _frozenset_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfFrozenSet(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _function_guard_intrinsic)(Thread* thread) {
  if (thread->stackTop().isFunction()) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _int_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfInt(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _int_check_exact_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPop();
  thread->stackSetTop(Bool::fromBool(arg.isSmallInt() || arg.isLargeInt()));
  return true;
}

bool FUNC(_builtins, _int_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfInt(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _list_append_intrinsic)(Thread* thread) {
  RawObject arg0 = thread->stackPeek(1);
  if (!thread->runtime()->isInstanceOfList(arg0)) {
    return false;
  }
  RawList self = arg0.rawCast<RawList>();
  word num_items = self.numItems();
  if (self.capacity() > num_items) {
    self.setNumItems(num_items + 1);
    self.atPut(num_items, thread->stackPeek(0));
    thread->stackDrop(2);
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _list_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfList(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _list_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isList()));
  return true;
}

RawObject FUNC(_builtins, _list_ctor)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kList), "unexpected cls");
  RawObject iterable_raw = args.get(1);
  if (iterable_raw == runtime->emptyTuple()) {
    return runtime->newList();
  }
  HandleScope scope(thread);
  Object iterable(&scope, iterable_raw);
  List self(&scope, runtime->newList());
  word src_length;
  Tuple src(&scope, runtime->emptyTuple());
  if (iterable.isList()) {
    src = List::cast(*iterable).items();
    src_length = List::cast(*iterable).numItems();
  } else if (iterable.isTuple()) {
    src = *iterable;
    src_length = src.length();
  } else {
    Object result(&scope, thread->invokeMethod2(self, ID(extend), iterable));
    if (result.isError()) return *result;
    return *self;
  }
  listExtend(thread, self, src, src_length);
  return *self;
}

bool FUNC(_builtins, _list_getitem_intrinsic)(Thread* thread) {
  RawObject arg0 = thread->stackPeek(1);
  if (!arg0.isList()) {
    return false;
  }
  RawObject arg1 = thread->stackPeek(0);
  word idx;
  if (arg1.isSmallInt()) {
    idx = SmallInt::cast(arg1).value();
  } else if (arg1.isBool()) {
    idx = Bool::cast(arg1).value();
  } else {
    return false;
  }
  RawList self = List::cast(arg0);
  if (0 <= idx && idx < self.numItems()) {
    thread->stackDrop(2);
    thread->stackSetTop(self.at(idx));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _list_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfList(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _list_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isList()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(List::cast(arg).numItems()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _list_setitem_intrinsic)(Thread* thread) {
  RawObject arg0 = thread->stackPeek(2);
  if (!arg0.isList()) {
    return false;
  }
  RawObject arg1 = thread->stackPeek(1);
  word idx;
  if (arg1.isSmallInt()) {
    idx = SmallInt::cast(arg1).value();
  } else if (arg1.isBool()) {
    idx = Bool::cast(arg1).value();
  } else {
    return false;
  }
  RawList self = List::cast(arg0);
  if (idx < 0 || idx >= self.numItems()) {
    return false;
  }
  self.atPut(idx, thread->stackPeek(0));
  thread->stackDrop(3);
  thread->stackSetTop(NoneType::object());
  return true;
}

bool FUNC(_builtins, _memoryview_guard_intrinsic)(Thread* thread) {
  if (thread->stackTop().isMemoryView()) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _range_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isRange()));
  return true;
}

bool FUNC(_builtins, _range_guard_intrinsic)(Thread* thread) {
  if (thread->stackTop().isRange()) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _seq_index_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      SmallInt::fromWord(SeqIterator::cast(thread->stackPop()).index()));
  return true;
}

bool FUNC(_builtins, _seq_iterable_intrinsic)(Thread* thread) {
  thread->stackSetTop(SeqIterator::cast(thread->stackPop()).iterable());
  return true;
}

bool FUNC(_builtins, _seq_set_index_intrinsic)(Thread* thread) {
  RawObject index = thread->stackPop();
  RawObject seq_iter = thread->stackPop();
  SeqIterator::cast(seq_iter).setIndex(Int::cast(index).asWord());
  return true;
}

bool FUNC(_builtins, _seq_set_iterable_intrinsic)(Thread* thread) {
  RawObject iterable = thread->stackPop();
  RawObject seq_iter = thread->stackPop();
  SeqIterator::cast(seq_iter).setIterable(iterable);
  return true;
}

bool FUNC(_builtins, _set_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfSet(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _set_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfSet(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _set_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isSet()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(Set::cast(arg).numItems()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _slice_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isSlice()));
  return true;
}

bool FUNC(_builtins, _slice_guard_intrinsic)(Thread* thread) {
  if (thread->stackTop().isSlice()) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _str_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfStr(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _str_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isStr()));
  return true;
}

bool FUNC(_builtins, _str_ctor_intrinsic)(Thread* thread) {
  // The type signature of _str_ctor is
  //   def _str_ctor(cls, obj=_Unbound, encoding=_Unbound, errors=_Unbound):
  //
  // `_str_ctor` is available internally so locating it in the stack tells us
  // how many arguments are given in the presence of optional arguments.
  RawObject callee =
      Type::cast(thread->runtime()->typeAt(LayoutId::kStr)).ctor();
  if (callee == thread->stackPeek(1)) {
    // Only `cls` is given: `str()` is executed.
    thread->stackDrop(1);
    thread->stackSetTop(Str::empty());
    return true;
  }
  if (callee == thread->stackPeek(2)) {
    // `cls` and `obj` are given: `str(s)` is executed.
    RawObject obj = thread->stackPeek(0);
    if (obj.isStr()) {
      thread->stackSetAt(2, obj);
      thread->stackDrop(2);
      return true;
    }
  }
  return false;
}

bool FUNC(_builtins, _str_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfStr(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _str_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isStr()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(Str::cast(arg).codePointLength()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _tuple_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfTuple(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _tuple_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isTuple()));
  return true;
}

bool FUNC(_builtins, _tuple_getitem_intrinsic)(Thread* thread) {
  RawObject arg0 = thread->stackPeek(1);
  if (!arg0.isTuple()) {
    return false;
  }
  RawObject arg1 = thread->stackPeek(0);
  word idx;
  if (arg1.isSmallInt()) {
    idx = SmallInt::cast(arg1).value();
  } else if (arg1.isBool()) {
    idx = Bool::cast(arg1).value();
  } else {
    return false;
  }
  RawTuple self = Tuple::cast(arg0);
  if (0 <= idx && idx < self.length()) {
    thread->stackDrop(2);
    thread->stackSetTop(self.at(idx));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _tuple_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfTuple(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _tuple_len_intrinsic)(Thread* thread) {
  RawObject arg = thread->stackPeek(0);
  if (arg.isTuple()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(Tuple::cast(arg).length()));
    return true;
  }
  return false;
}

bool FUNC(_builtins, _type_intrinsic)(Thread* thread) {
  thread->stackSetTop(thread->runtime()->typeOf(thread->stackPop()));
  return true;
}

bool FUNC(_builtins, _type_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(
      Bool::fromBool(thread->runtime()->isInstanceOfType(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _type_check_exact_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(thread->stackPop().isType()));
  return true;
}

bool FUNC(_builtins, _type_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfType(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _type_subclass_guard_intrinsic)(Thread* thread) {
  RawObject subclass = thread->stackPeek(0);
  RawObject superclass = thread->stackPeek(1);
  if (subclass == superclass && subclass.isType()) {
    thread->stackDrop(2);
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

bool FUNC(_builtins, _weakref_check_intrinsic)(Thread* thread) {
  thread->stackSetTop(Bool::fromBool(
      thread->runtime()->isInstanceOfWeakRef(thread->stackPop())));
  return true;
}

bool FUNC(_builtins, _weakref_guard_intrinsic)(Thread* thread) {
  if (thread->runtime()->isInstanceOfWeakRef(thread->stackTop())) {
    thread->stackPop();
    thread->stackSetTop(NoneType::object());
    return true;
  }
  return false;
}

void FUNC(_builtins, __init_module__)(Thread* thread, const Module& module,
                                      View<byte> bytecode) {
  HandleScope scope(thread);
  Object unbound_value(&scope, Unbound::object());
  moduleAtPutById(thread, module, ID(_Unbound), unbound_value);

  Object compile_flags_mask(&scope,
                            SmallInt::fromWord(Code::kCompileFlagsMask));
  moduleAtPutById(thread, module, ID(_compile_flags_mask), compile_flags_mask);

  Object maxunicode(&scope, SmallInt::fromWord(kMaxUnicode));
  moduleAtPutById(thread, module, ID(maxunicode), maxunicode);

  // We did not initialize the `builtins` module yet, so we point
  // `__builtins__` to this module instead.
  moduleAtPutById(thread, module, ID(__builtins__), module);

  executeFrozenModule(thread, module, bytecode);
}

RawObject FUNC(_builtins, _ContextVar_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isContextVar()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(ContextVar));
}

RawObject FUNC(_builtins, _Token_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isToken()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(Token));
}

RawObject FUNC(_builtins, _address)(Thread* thread, Arguments args) {
  return thread->runtime()->newInt(args.get(0).raw());
}

RawObject FUNC(_builtins, _anyset_check)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  RawObject arg = args.get(0);
  return Bool::fromBool(runtime->isInstanceOfSet(arg) ||
                        runtime->isInstanceOfFrozenSet(arg));
}

RawObject FUNC(_builtins, _async_generator_guard)(Thread* thread,
                                                  Arguments args) {
  if (args.get(0).isAsyncGenerator()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(async_generator));
}

RawObject FUNC(_builtins, _base_exception_cause)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  return exc.cause();
}

RawObject FUNC(_builtins, _base_exception_context)(Thread* thread,
                                                   Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  return exc.context();
}

RawObject FUNC(_builtins, _base_exception_set_cause)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  Object value(&scope, args.get(1));
  if (!value.isNoneType() && !runtime->isInstanceOfBaseException(*value)) {
    return thread->raiseRequiresType(value, ID(BaseException));
  }
  exc.setCause(*value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _base_exception_set_context)(Thread* thread,
                                                       Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  Object value(&scope, args.get(1));
  if (!value.isNoneType() && !runtime->isInstanceOfBaseException(*value)) {
    return thread->raiseRequiresType(value, ID(BaseException));
  }
  exc.setContext(*value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _base_exception_set_traceback)(Thread* thread,
                                                         Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  Object value(&scope, args.get(1));
  if (!value.isNoneType() && !value.isTraceback()) {
    return thread->raiseRequiresType(value, ID(traceback));
  }
  exc.setTraceback(*value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _base_exception_traceback)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));
  if (!runtime->isInstanceOfBaseException(*self)) {
    return thread->raiseRequiresType(self, ID(BaseException));
  }
  BaseException exc(&scope, *self);
  return exc.traceback();
}

RawObject FUNC(_builtins, _bool_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isBool());
}

RawObject FUNC(_builtins, _bool_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isBool()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(bool));
}

RawObject FUNC(_builtins, _bound_method)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object function(&scope, args.get(0));
  Object owner(&scope, args.get(1));
  return thread->runtime()->newBoundMethod(function, owner);
}

RawObject FUNC(_builtins, _bound_method_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isBoundMethod()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(method));
}

RawObject FUNC(_builtins, _builtin_type)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object name(&scope, args.get(0));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object result(&scope, findBuiltinTypeWithName(thread, name));
  CHECK(!result.isErrorNotFound(), "Built-in type not found");
  return *result;
}

RawObject FUNC(_builtins, _byte_guard)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object obj(&scope, args.get(0));
  if (runtime->isInstanceOfBytes(*obj)) {
    Bytes bytes(&scope, bytesUnderlying(*obj));
    if (bytes.length() == 1) {
      return SmallInt::fromWord(bytes.byteAt(0));
    }
  } else if (runtime->isInstanceOfBytearray(*obj)) {
    Bytearray array(&scope, *obj);
    if (array.numItems() == 1) {
      return SmallInt::fromWord(array.byteAt(0));
    }
  }
  Function function(&scope,
                    thread->currentFrame()->previousFrame()->function());
  Str function_name(&scope, function.name());
  return thread->raiseWithFmt(
      LayoutId::kTypeError,
      "%S() argument 2 must be a byte string of length 1, not %T",
      &function_name, &obj);
}

RawObject FUNC(_builtins, _bytearray_append)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }
  Bytearray self(&scope, *self_obj);
  Object item_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*item_obj)) {
    return Unbound::object();
  }
  OptInt<byte> item_opt = intUnderlying(*item_obj).asInt<byte>();
  if (item_opt.error != CastError::None) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  bytearrayAdd(thread, runtime, self, item_opt.value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytearray_clear)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  self.downsize(0);
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytearray_contains)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }
  Object key_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*key_obj)) {
    return Unbound::object();
  }
  OptInt<byte> key_opt = intUnderlying(*key_obj).asInt<byte>();
  if (key_opt.error != CastError::None) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  Bytearray self(&scope, *self_obj);
  MutableBytes bytes(&scope, self.items());
  return Bool::fromBool(bytes.findByte(key_opt.value, 0, self.numItems()) >= 0);
}

RawObject FUNC(_builtins, _bytearray_contains_byteslike)(Thread* thread,
                                                         Arguments args) {
  HandleScope scope(thread);

  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }

  Byteslike key(&scope, thread, args.get(1));
  if (!key.isValid()) {
    Object key_obj(&scope, args.get(1));
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "a bytes-like object is required, not '%T'",
                                &key_obj);
  }

  Bytearray self(&scope, *self_obj);
  MutableBytes bytes(&scope, self.items());

  if (key.length() == 0) {
    // CPython returns true for: b'' in b'abc'.
    return Bool::fromBool(true);
  }

  return Bool::fromBool(
      Utils::memoryFind(reinterpret_cast<byte*>(bytes.address()),
                        bytes.length(), reinterpret_cast<byte*>(key.address()),
                        key.length()) != -1);
}

RawObject FUNC(_builtins, _bytearray_copy)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }
  Bytearray self(&scope, *self_obj);
  Bytes src(&scope, self.items());
  MutableBytes dst(&scope, runtime->mutableBytesFromBytes(thread, src));
  Bytearray result(&scope, runtime->newBytearray());
  result.setItems(*dst);
  result.setNumItems(self.numItems());
  return *result;
}

RawObject FUNC(_builtins, _bytearray_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfBytearray(args.get(0)));
}

RawObject FUNC(_builtins, _bytearray_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfBytearray(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(bytearray));
}

RawObject FUNC(_builtins, _bytearray_delitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  word length = self.numItems();
  word idx = intUnderlying(args.get(1)).asWordSaturated();
  if (idx < 0) {
    idx += length;
  }
  if (idx < 0 || idx >= length) {
    return thread->raiseWithFmt(LayoutId::kIndexError,
                                "bytearray index out of range");
  }
  word last_idx = length - 1;
  MutableBytes self_bytes(&scope, self.items());
  self_bytes.replaceFromWithStartAt(idx, DataArray::cast(self.items()),
                                    last_idx - idx, idx + 1);
  self.setNumItems(last_idx);
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytearray_delslice)(Thread* thread, Arguments args) {
  // This function deletes elements that are specified by a slice by copying.
  // It compacts to the left elements in the slice range and then copies
  // elements after the slice into the free area.  The self element count is
  // decremented and elements in the unused part of the self are overwritten
  // with None.
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));

  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();

  word slice_length = Slice::length(start, stop, step);
  DCHECK_BOUND(slice_length, self.numItems());
  if (slice_length == 0) {
    // Nothing to delete
    return NoneType::object();
  }
  if (slice_length == self.numItems()) {
    // Delete all the items
    self.setNumItems(0);
    return NoneType::object();
  }
  if (step < 0) {
    // Adjust step to make iterating easier
    start = start + step * (slice_length - 1);
    step = -step;
  }
  DCHECK_INDEX(start, self.numItems());
  DCHECK(step <= self.numItems() || slice_length == 1,
         "Step should be in bounds or only one element should be sliced");
  // Sliding compaction of elements out of the slice to the left
  // Invariant: At each iteration of the loop, `fast` is the index of an
  // element addressed by the slice.
  // Invariant: At each iteration of the inner loop, `slow` is the index of a
  // location to where we are relocating a slice addressed element. It is *not*
  // addressed by the slice.
  word fast = start;
  MutableBytes self_bytes(&scope, self.items());
  for (word i = 1; i < slice_length; i++) {
    DCHECK_INDEX(fast, self.numItems());
    word slow = fast + 1;
    fast += step;
    for (; slow < fast; slow++) {
      self_bytes.byteAtPut(slow - i, self_bytes.byteAt(slow));
    }
  }
  // Copy elements into the space where the deleted elements were
  for (word i = fast + 1; i < self.numItems(); i++) {
    self_bytes.byteAtPut(i - slice_length, self_bytes.byteAt(i));
  }
  self.setNumItems(self.numItems() - slice_length);
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytearray_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }
  Bytearray self(&scope, *self_obj);
  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    key = intUnderlying(*key);
    if (key.isLargeInt()) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }
    word index = Int::cast(*key).asWord();
    word length = self.numItems();
    if (index < 0 || index >= length) {
      if (index < 0) {
        index += length;
      }
      if (index < 0 || index >= length) {
        return thread->raiseWithFmt(LayoutId::kIndexError,
                                    "bytearray index out of range");
      }
    }
    return SmallInt::fromWord(self.byteAt(index));
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  word result_len = Slice::adjustIndices(self.numItems(), &start, &stop, 1);
  if (result_len == 0) {
    return runtime->newBytearray();
  }

  Bytearray result(&scope, runtime->newBytearray());
  MutableBytes result_bytes(&scope,
                            runtime->newMutableBytesUninitialized(result_len));
  MutableBytes src_bytes(&scope, self.items());
  result_bytes.replaceFromWithStartAt(0, *src_bytes, result_len, start);
  result.setItems(*result_bytes);
  result.setNumItems(result_len);
  return *result;
}

RawObject FUNC(_builtins, _bytearray_getslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  word len = Slice::length(start, stop, step);
  Runtime* runtime = thread->runtime();
  Bytearray result(&scope, runtime->newBytearray());
  runtime->bytearrayEnsureCapacity(thread, result, len);
  result.setNumItems(len);
  for (word i = 0, idx = start; i < len; i++, idx += step) {
    result.byteAtPut(i, self.byteAt(idx));
  }
  return *result;
}

RawObject FUNC(_builtins, _bytearray_ljust)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }

  word width;
  Object width_obj(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*width_obj)) {
    width = intUnderlying(args.get(1)).asWordSaturated();
    if (!SmallInt::isValid(width)) {
      return thread->raiseWithFmt(
          LayoutId::kOverflowError,
          "Python int too large to convert to C ssize_t");
    }
  } else {
    return Unbound::object();
  }

  byte fill;
  Object fillbyte_obj(&scope, args.get(2));
  if (runtime->isInstanceOfBytes(*fillbyte_obj)) {
    Bytes fillbyte(&scope, bytesUnderlying(*fillbyte_obj));
    if (fillbyte.length() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else if (runtime->isInstanceOfBytearray(*fillbyte_obj)) {
    Bytearray fillbyte(&scope, *fillbyte_obj);
    if (fillbyte.numItems() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else {
    return Unbound::object();
  }

  Bytearray self(&scope, *self_obj);
  word self_length = self.numItems();
  word result_length = Utils::maximum(width, self_length);
  MutableBytes buffer(&scope,
                      runtime->newMutableBytesUninitialized(result_length));
  buffer.replaceFromWith(0, DataArray::cast(self.items()), self_length);
  buffer.replaceFromWithByte(self_length, fill, result_length - self_length);

  Bytearray result(&scope, runtime->newBytearray());
  result.setItems(*buffer);
  result.setNumItems(result_length);
  return *result;
}

RawObject FUNC(_builtins, _bytearray_rjust)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytearray(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }

  word width;
  Object width_obj(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*width_obj)) {
    width = intUnderlying(args.get(1)).asWordSaturated();
    if (!SmallInt::isValid(width)) {
      return thread->raiseWithFmt(
          LayoutId::kOverflowError,
          "Python int too large to convert to C ssize_t");
    }
  } else {
    return Unbound::object();
  }

  byte fill;
  Object fillbyte_obj(&scope, args.get(2));
  if (runtime->isInstanceOfBytes(*fillbyte_obj)) {
    Bytes fillbyte(&scope, bytesUnderlying(*fillbyte_obj));
    if (fillbyte.length() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else if (runtime->isInstanceOfBytearray(*fillbyte_obj)) {
    Bytearray fillbyte(&scope, *fillbyte_obj);
    if (fillbyte.numItems() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else {
    return Unbound::object();
  }

  Bytearray self(&scope, *self_obj);
  word self_length = self.numItems();
  word result_length = Utils::maximum(width, self_length);
  word pad_length = result_length - self_length;
  MutableBytes buffer(&scope,
                      runtime->newMutableBytesUninitialized(result_length));
  buffer.replaceFromWithByte(0, fill, pad_length);
  buffer.replaceFromWith(pad_length, DataArray::cast(self.items()),
                         self_length);

  Bytearray result(&scope, runtime->newBytearray());
  result.setItems(*buffer);
  result.setNumItems(result_length);
  return *result;
}

RawObject FUNC(_builtins, _bytearray_setitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  word index = intUnderlying(args.get(1)).asWordSaturated();
  if (!SmallInt::isValid(index)) {
    Object key_obj(&scope, args.get(1));
    return thread->raiseWithFmt(LayoutId::kIndexError,
                                "cannot fit '%T' into an index-sized integer",
                                &key_obj);
  }
  word length = self.numItems();
  if (index < 0) {
    index += length;
  }
  if (index < 0 || index >= length) {
    return thread->raiseWithFmt(LayoutId::kIndexError, "index out of range");
  }
  word val = intUnderlying(args.get(2)).asWordSaturated();
  if (val < 0 || val > kMaxByte) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  self.byteAtPut(index, val);
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytearray_setslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  Byteslike src(&scope, thread, args.get(4));
  DCHECK(src.isValid(), "argument must be a byteslike");

  // Make sure that the degenerate case of a slice assignment where start is
  // greater than stop inserts before the start and not the stop. For example,
  // b[5:2] = ... should inserts before 5, not before 2.
  if ((step < 0 && start < stop) || (step > 0 && start > stop)) {
    stop = start;
  }

  Runtime* runtime = thread->runtime();
  word src_length = src.length();

  if (step == 1) {
    word growth = src_length - (stop - start);
    word new_length = self.numItems() + growth;
    if (self == args.get(4)) {
      // Rare case when replacing lhs with elements of rhs when lhs == rhs.
      // Will always have growth >= 0.
      if (growth == 0) {
        return NoneType::object();
      }
      runtime->bytearrayEnsureCapacity(thread, self, new_length);
      self.setNumItems(new_length);
      MutableBytes dst_bytes(&scope, self.items());
      dst_bytes.replaceFromWith(start, *dst_bytes, src_length);
      dst_bytes.replaceFromWithStartAt(start + src_length, *dst_bytes,
                                       src_length - stop, start + stop);
      return NoneType::object();
    }
    if (growth == 0) {
      // Assignment does not change the length of the bytearray. Do nothing.
    } else if (growth > 0) {
      // Assignment grows the length of the bytearray. Ensure there is enough
      // free space in the underlying tuple for the new bytes and move stuff
      // out of the way.
      runtime->bytearrayEnsureCapacity(thread, self, new_length);
      // Make the free space part of the bytearray. Must happen before shifting
      // so we can index into the free space.
      self.setNumItems(new_length);
      // Shift some bytes to the right.
      self.replaceFromWithStartAt(start + growth, *self,
                                  new_length - growth - start, start);
    } else {
      // Growth is negative so assignment shrinks the length of the bytearray.
      // Shift some bytes to the left.
      self.replaceFromWithStartAt(start, *self, new_length - start,
                                  start - growth);
      // Remove the free space from the length of the bytearray. Must happen
      // after shifting and clearing so we can index into the free space.
      self.setNumItems(new_length);
    }
    MutableBytes dst_bytes(&scope, self.items());
    // Copy new elements into the middle
    dst_bytes.replaceFromWithByteslike(start, src, src_length);
    return NoneType::object();
  }

  word slice_length = Slice::length(start, stop, step);
  if (slice_length != src_length) {
    return thread->raiseWithFmt(
        LayoutId::kValueError,
        "attempt to assign bytes of size %w to extended slice of size %w",
        src_length, slice_length);
  }

  MutableBytes dst_bytes(&scope, self.items());
  for (word dst_idx = start, src_idx = 0; src_idx < src_length;
       dst_idx += step, src_idx++) {
    dst_bytes.byteAtPut(dst_idx, src.byteAt(src_idx));
  }
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytes_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfBytes(args.get(0)));
}

RawObject FUNC(_builtins, _bytes_contains)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytes(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytes));
  }
  Object key_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*key_obj)) {
    return Unbound::object();
  }
  OptInt<byte> key_opt = intUnderlying(*key_obj).asInt<byte>();
  if (key_opt.error != CastError::None) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  Bytes self(&scope, *self_obj);
  return Bool::fromBool(self.findByte(key_opt.value, 0, self.length()) >= 0);
}

RawObject FUNC(_builtins, _bytes_decode)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object bytes_obj(&scope, args.get(0));
  if (!bytes_obj.isBytes()) {
    return Unbound::object();
  }
  Bytes bytes(&scope, *bytes_obj);
  static RawSmallStr ascii = SmallStr::fromCStr("ascii");
  static RawSmallStr utf8 = SmallStr::fromCStr("utf-8");
  static RawSmallStr latin1 = SmallStr::fromCStr("latin-1");
  Str enc(&scope, args.get(1));
  if (enc != ascii && enc != utf8 && enc != latin1 &&
      enc.compareCStr("iso-8859-1") != 0) {
    return Unbound::object();
  }
  return bytesDecodeASCII(thread, bytes);
}

RawObject FUNC(_builtins, _bytes_decode_ascii)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object bytes_obj(&scope, args.get(0));
  if (!bytes_obj.isBytes()) {
    return Unbound::object();
  }
  Bytes bytes(&scope, *bytes_obj);
  return bytesDecodeASCII(thread, bytes);
}

RawObject FUNC(_builtins, _bytes_decode_utf_8)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object bytes_obj(&scope, args.get(0));
  if (!bytes_obj.isBytes()) {
    return Unbound::object();
  }
  Bytes bytes(&scope, *bytes_obj);
  return bytesDecodeASCII(thread, bytes);
}

RawObject FUNC(_builtins, _bytes_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfBytes(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(bytes));
}

RawObject FUNC(_builtins, _bytearray_join)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object sep_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfBytearray(*sep_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytearray));
  }
  Bytearray sep(&scope, args.get(0));
  Bytes sep_bytes(&scope, sep.items());
  Object iterable(&scope, args.get(1));
  Tuple tuple(&scope, runtime->emptyTuple());
  word length;
  if (iterable.isList()) {
    tuple = List::cast(*iterable).items();
    length = List::cast(*iterable).numItems();
  } else if (iterable.isTuple()) {
    tuple = *iterable;
    length = tuple.length();
  } else {
    // Collect items into list in Python and call again
    return Unbound::object();
  }
  Object joined(&scope,
                bytesJoin(thread, sep_bytes, sep.numItems(), tuple, length));
  if (joined.isErrorException()) {
    return *joined;
  }
  Bytearray result(&scope, runtime->newBytearray());
  result.setItems(*joined);
  result.setNumItems(Bytes::cast(*joined).length());
  return *result;
}

RawObject FUNC(_builtins, _bytearray_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytearray self(&scope, args.get(0));
  return SmallInt::fromWord(self.numItems());
}

RawObject FUNC(_builtins, _bytes_from_bytes)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  DCHECK(type.builtinBase() == LayoutId::kBytes, "type must subclass bytes");
  Object value(&scope, bytesUnderlying(args.get(1)));
  if (type.isBuiltin()) return *value;
  Layout type_layout(&scope, type.instanceLayout());
  UserBytesBase instance(&scope, thread->runtime()->newInstance(type_layout));
  instance.setValue(*value);
  return *instance;
}

RawObject FUNC(_builtins, _bytes_from_ints)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object src(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  // TODO(T38246066): buffers other than bytes, bytearray
  if (runtime->isInstanceOfBytes(*src)) {
    return *src;
  }
  if (runtime->isInstanceOfBytearray(*src)) {
    Bytearray source(&scope, *src);
    return bytearrayAsBytes(thread, source);
  }
  if (src.isList()) {
    List source(&scope, *src);
    Tuple items(&scope, source.items());
    return runtime->bytesFromTuple(thread, items, source.numItems());
  }
  if (src.isTuple()) {
    Tuple source(&scope, *src);
    return runtime->bytesFromTuple(thread, source, source.length());
  }
  if (runtime->isInstanceOfStr(*src)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "cannot convert '%T' object to bytes", &src);
  }
  // Slow path: iterate over source in Python, collect into list, and call again
  return NoneType::object();
}

RawObject FUNC(_builtins, _bytes_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytes(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytes));
  }
  Bytes self(&scope, bytesUnderlying(*self_obj));

  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    key = intUnderlying(*key);
    if (key.isLargeInt()) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }
    word index = Int::cast(*key).asWord();
    word length = self.length();
    if (index < 0 || index >= length) {
      if (index < 0) {
        index += length;
      }
      if (index < 0 || index >= length) {
        return thread->raiseWithFmt(LayoutId::kIndexError,
                                    "index out of range");
      }
    }
    return SmallInt::fromWord(self.byteAt(index));
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  word result_len = Slice::adjustIndices(self.length(), &start, &stop, 1);
  return bytesSubseq(thread, self, start, result_len);
}

RawObject FUNC(_builtins, _bytes_getslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytes self(&scope, bytesUnderlying(args.get(0)));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  return thread->runtime()->bytesSlice(thread, self, start, stop, step);
}

RawObject FUNC(_builtins, _bytes_join)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfBytes(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytes));
  }
  Bytes self(&scope, bytesUnderlying(*self_obj));
  Object iterable(&scope, args.get(1));
  Tuple tuple(&scope, runtime->emptyTuple());
  word length;
  if (iterable.isList()) {
    tuple = List::cast(*iterable).items();
    length = List::cast(*iterable).numItems();
  } else if (iterable.isTuple()) {
    tuple = *iterable;
    length = Tuple::cast(*iterable).length();
  } else {
    // Collect items into list in Python and call again
    return Unbound::object();
  }
  return bytesJoin(thread, self, self.length(), tuple, length);
}

RawObject FUNC(_builtins, _bytes_len)(Thread*, Arguments args) {
  return SmallInt::fromWord(bytesUnderlying(args.get(0)).length());
}

RawObject FUNC(_builtins, _bytes_ljust)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfBytes(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytes));
  }

  Object width_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*width_obj)) {
    return Unbound::object();
  }

  Int width_int(&scope, intUnderlying(*width_obj));
  if (width_int.isLargeInt()) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "Python int too large to convert to C ssize_t");
  }
  word width = width_int.asWord();

  byte fill;
  Object fillbyte_obj(&scope, args.get(2));
  if (runtime->isInstanceOfBytes(*fillbyte_obj)) {
    Bytes fillbyte(&scope, bytesUnderlying(*fillbyte_obj));
    if (fillbyte.length() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else if (runtime->isInstanceOfBytearray(*fillbyte_obj)) {
    Bytearray fillbyte(&scope, *fillbyte_obj);
    if (fillbyte.numItems() != 1) {
      return Unbound::object();
    }
    fill = fillbyte.byteAt(0);
  } else {
    return Unbound::object();
  }

  Bytes self(&scope, *self_obj);
  word self_length = self.length();
  if (self_length >= width) {
    return *self_obj;
  }

  if (width <= SmallBytes::kMaxLength) {
    byte buffer[SmallBytes::kMaxLength];

    std::memset(buffer, fill, SmallBytes::kMaxLength);
    self.copyTo(buffer, self_length);

    return SmallBytes::fromBytes({buffer, width});
  }

  MutableBytes buffer(&scope, runtime->newMutableBytesUninitialized(width));
  buffer.replaceFromWithBytes(0, *self, self_length);
  buffer.replaceFromWithByte(self_length, fill, width - self_length);

  return buffer.becomeImmutable();
}

RawObject FUNC(_builtins, _bytes_maketrans)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object from_obj(&scope, args.get(0));
  Object to_obj(&scope, args.get(1));
  word length;
  Runtime* runtime = thread->runtime();
  if (runtime->isInstanceOfBytes(*from_obj)) {
    Bytes bytes(&scope, bytesUnderlying(*from_obj));
    length = bytes.length();
    from_obj = *bytes;
  } else if (runtime->isInstanceOfBytearray(*from_obj)) {
    Bytearray array(&scope, *from_obj);
    length = array.numItems();
    from_obj = array.items();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes or bytearray");
  }
  if (runtime->isInstanceOfBytes(*to_obj)) {
    Bytes bytes(&scope, bytesUnderlying(*to_obj));
    DCHECK(bytes.length() == length, "lengths should already be the same");
    to_obj = *bytes;
  } else if (runtime->isInstanceOfBytearray(*to_obj)) {
    Bytearray array(&scope, *to_obj);
    DCHECK(array.numItems() == length, "lengths should already be the same");
    to_obj = array.items();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes or bytearray");
  }
  Bytes from(&scope, *from_obj);
  Bytes to(&scope, *to_obj);
  byte table[kByteTranslationTableLength];
  for (word i = 0; i < kByteTranslationTableLength; i++) {
    table[i] = i;
  }
  for (word i = 0; i < length; i++) {
    table[from.byteAt(i)] = to.byteAt(i);
  }
  return runtime->newBytesWithAll(table);
}

RawObject FUNC(_builtins, _bytes_repeat)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytes self(&scope, bytesUnderlying(args.get(0)));
  // TODO(T55084422): unify bounds checking
  word count = intUnderlying(args.get(1)).asWordSaturated();
  if (!SmallInt::isValid(count)) {
    Object count_obj(&scope, args.get(1));
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "cannot fit '%T' into an index-sized integer",
                                &count_obj);
  }
  // NOTE: unlike __mul__, we raise a value error for negative count
  if (count < 0) {
    return thread->raiseWithFmt(LayoutId::kValueError, "negative count");
  }
  return thread->runtime()->bytesRepeat(thread, self, self.length(), count);
}

RawObject FUNC(_builtins, _bytes_replace)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Object old_bytes_obj(&scope, args.get(1));
  Object new_bytes_obj(&scope, args.get(2));
  Object count_obj(&scope, args.get(3));

  // Type Checks
  if (!runtime->isInstanceOfBytes(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(bytes));
  }
  if (!runtime->isByteslike(*old_bytes_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "a bytes-like object is required, not '%T'",
                                &old_bytes_obj);
  }
  if (!runtime->isByteslike(*new_bytes_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "a bytes-like object is required, not '%T'",
                                &new_bytes_obj);
  }
  if (runtime->isInstanceOfFloat(*count_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "integer argument expected, got float",
                                &count_obj);
  }
  if (!runtime->isInstanceOfInt(*count_obj)) {
    return Unbound::object();
  }
  if (!count_obj.isSmallInt()) {
    UNIMPLEMENTED("handle if count is a LargeInt");
  }

  // Byteslike breakdown for oldbytes and newbytes
  word old_bytes_len;
  if (runtime->isInstanceOfBytes(*old_bytes_obj)) {
    Bytes bytes(&scope, bytesUnderlying(*old_bytes_obj));
    old_bytes_obj = *bytes;
    old_bytes_len = bytes.length();
  } else if (runtime->isInstanceOfBytearray(*old_bytes_obj)) {
    Bytearray bytearray(&scope, *old_bytes_obj);
    old_bytes_obj = bytearray.items();
    old_bytes_len = bytearray.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    UNIMPLEMENTED("bytes-like other than bytes or bytearray");
  }
  word new_bytes_len;
  if (runtime->isInstanceOfBytes(*new_bytes_obj)) {
    Bytes bytes(&scope, bytesUnderlying(*new_bytes_obj));
    new_bytes_obj = *bytes;
    new_bytes_len = bytes.length();
  } else if (runtime->isInstanceOfBytearray(*new_bytes_obj)) {
    Bytearray bytearray(&scope, *new_bytes_obj);
    new_bytes_obj = bytearray.items();
    new_bytes_len = bytearray.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    UNIMPLEMENTED("bytes-like other than bytes or bytearray");
  }

  Bytes self(&scope, *self_obj);
  Bytes old_bytes(&scope, *old_bytes_obj);
  Bytes new_bytes(&scope, *new_bytes_obj);
  word count = intUnderlying(*count_obj).asWordSaturated();
  return runtime->bytesReplace(thread, self, old_bytes, old_bytes_len,
                               new_bytes, new_bytes_len, count);
}

RawObject FUNC(_builtins, _bytes_split)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Bytes self(&scope, bytesUnderlying(args.get(0)));
  Object sep_obj(&scope, args.get(1));
  Int maxsplit_int(&scope, intUnderlying(args.get(2)));
  if (maxsplit_int.numDigits() > 1) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "Python int too large to convert to C ssize_t");
  }
  word maxsplit = maxsplit_int.asWord();
  if (maxsplit < 0) {
    maxsplit = kMaxWord;
  }
  word sep_len;
  Runtime* runtime = thread->runtime();
  if (runtime->isInstanceOfBytes(*sep_obj)) {
    Bytes sep(&scope, bytesUnderlying(*sep_obj));
    sep_obj = *sep;
    sep_len = sep.length();
  } else if (runtime->isInstanceOfBytearray(*sep_obj)) {
    Bytearray sep(&scope, *sep_obj);
    sep_obj = sep.items();
    sep_len = sep.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    UNIMPLEMENTED("bytes-like other than bytes or bytearray");
  }
  if (sep_len == 0) {
    return thread->raiseWithFmt(LayoutId::kValueError, "empty separator");
  }
  Bytes sep(&scope, *sep_obj);
  word self_len = self.length();

  // First pass: calculate the length of the result list.
  word splits = 0;
  word start = 0;
  while (splits < maxsplit) {
    word end = bytesFind(self, self_len, sep, sep_len, start, self_len);
    if (end < 0) {
      break;
    }
    splits++;
    start = end + sep_len;
  }
  word result_len = splits + 1;

  // Second pass: write subsequences into result list.
  List result(&scope, runtime->newList());
  MutableTuple buffer(&scope, runtime->newMutableTuple(result_len));
  start = 0;
  for (word i = 0; i < splits; i++) {
    word end = bytesFind(self, self_len, sep, sep_len, start, self_len);
    DCHECK(end != -1, "already found in first pass");
    buffer.atPut(i, bytesSubseq(thread, self, start, end - start));
    start = end + sep_len;
  }
  buffer.atPut(splits, bytesSubseq(thread, self, start, self_len - start));
  result.setItems(*buffer);
  result.setNumItems(result_len);
  return *result;
}

RawObject FUNC(_builtins, _bytes_split_whitespace)(Thread* thread,
                                                   Arguments args) {
  HandleScope scope(thread);
  Bytes self(&scope, bytesUnderlying(args.get(0)));
  Int maxsplit_int(&scope, intUnderlying(args.get(1)));
  if (maxsplit_int.numDigits() > 1) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "Python int too large to convert to C ssize_t");
  }
  word self_len = self.length();
  word maxsplit = maxsplit_int.asWord();
  if (maxsplit < 0) {
    maxsplit = kMaxWord;
  }

  // First pass: calculate the length of the result list.
  word splits = 0;
  word index = 0;
  while (splits < maxsplit) {
    while (index < self_len && ASCII::isSpace(self.byteAt(index))) {
      index++;
    }
    if (index == self_len) break;
    index++;
    while (index < self_len && !ASCII::isSpace(self.byteAt(index))) {
      index++;
    }
    splits++;
  }
  while (index < self_len && ASCII::isSpace(self.byteAt(index))) {
    index++;
  }
  bool has_remaining = index < self_len;
  word result_len = has_remaining ? splits + 1 : splits;

  // Second pass: write subsequences into result list.
  Runtime* runtime = thread->runtime();
  List result(&scope, runtime->newList());
  if (result_len == 0) return *result;
  MutableTuple buffer(&scope, runtime->newMutableTuple(result_len));
  index = 0;
  for (word i = 0; i < splits; i++) {
    while (ASCII::isSpace(self.byteAt(index))) {
      index++;
    }
    word start = index++;
    while (index < self_len && !ASCII::isSpace(self.byteAt(index))) {
      index++;
    }
    buffer.atPut(i, bytesSubseq(thread, self, start, index - start));
  }
  if (has_remaining) {
    while (ASCII::isSpace(self.byteAt(index))) {
      index++;
    }
    buffer.atPut(splits, bytesSubseq(thread, self, index, self_len - index));
  }
  result.setItems(*buffer);
  result.setNumItems(result_len);
  return *result;
}

RawObject FUNC(_builtins, _byteslike_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isByteslike(args.get(0)));
}

RawObject FUNC(_builtins, _byteslike_compare_digest)(Thread* thread,
                                                     Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object left_obj(&scope, args.get(0));
  Object right_obj(&scope, args.get(1));
  DCHECK(runtime->isInstanceOfBytes(*left_obj) ||
             runtime->isInstanceOfBytearray(*left_obj),
         "_byteslike_compare_digest requires 'bytes' or 'bytearray' instance");
  DCHECK(runtime->isInstanceOfBytes(*right_obj) ||
             runtime->isInstanceOfBytearray(*right_obj),
         "_byteslike_compare_digest requires 'bytes' or 'bytearray' instance");
  // TODO(T57794178): Use volatile
  Bytes left(&scope, Bytes::empty());
  Bytes right(&scope, Bytes::empty());
  word left_len = 0;
  word right_len = 0;
  if (runtime->isInstanceOfBytes(*left_obj)) {
    left = bytesUnderlying(*left_obj);
    left_len = left.length();
  } else {
    Bytearray byte_array(&scope, *left_obj);
    left = byte_array.items();
    left_len = byte_array.numItems();
  }
  if (runtime->isInstanceOfBytes(*right_obj)) {
    right = bytesUnderlying(*right_obj);
    right_len = right.length();
  } else {
    Bytearray byte_array(&scope, *right_obj);
    right = byte_array.items();
    right_len = byte_array.numItems();
  }
  word length = Utils::minimum(left_len, right_len);
  word result = (right_len == left_len) ? 0 : 1;
  for (word i = 0; i < length; i++) {
    result |= left.byteAt(i) ^ right.byteAt(i);
  }
  return Bool::fromBool(result == 0);
}

RawObject FUNC(_builtins, _byteslike_count)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  word haystack_len;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes self(&scope, bytesUnderlying(*self_obj));
    self_obj = *self;
    haystack_len = self.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    self_obj = self.items();
    haystack_len = self.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Object sub_obj(&scope, args.get(1));
  word needle_len;
  if (runtime->isInstanceOfBytes(*sub_obj)) {
    Bytes sub(&scope, bytesUnderlying(*sub_obj));
    sub_obj = *sub;
    needle_len = sub.length();
  } else if (runtime->isInstanceOfBytearray(*sub_obj)) {
    Bytearray sub(&scope, *sub_obj);
    sub_obj = sub.items();
    needle_len = sub.numItems();
  } else if (runtime->isInstanceOfInt(*sub_obj)) {
    word sub = intUnderlying(*sub_obj).asWordSaturated();
    if (sub < 0 || sub > kMaxByte) {
      return thread->raiseWithFmt(LayoutId::kValueError,
                                  "byte must be in range(0, 256)");
    }
    sub_obj = runtime->newBytes(1, sub);
    needle_len = 1;
  } else {
    // TODO(T38246066): support buffer protocol
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Bytes haystack(&scope, *self_obj);
  Bytes needle(&scope, *sub_obj);
  Object start_obj(&scope, args.get(2));
  Object stop_obj(&scope, args.get(3));
  word start = intUnderlying(*start_obj).asWordSaturated();
  word end = intUnderlying(*stop_obj).asWordSaturated();
  return SmallInt::fromWord(
      bytesCount(haystack, haystack_len, needle, needle_len, start, end));
}

RawObject FUNC(_builtins, _byteslike_endswith)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  word self_len;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes self(&scope, bytesUnderlying(*self_obj));
    self_obj = *self;
    self_len = self.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    self_obj = self.items();
    self_len = self.numItems();
  } else {
    UNREACHABLE("self has an unexpected type");
  }
  DCHECK(self_obj.isBytes(),
         "bytes-like object not resolved to underlying bytes");
  Object suffix_obj(&scope, args.get(1));
  word suffix_len;
  if (runtime->isInstanceOfBytes(*suffix_obj)) {
    Bytes suffix(&scope, bytesUnderlying(*suffix_obj));
    suffix_obj = *suffix;
    suffix_len = suffix.length();
  } else if (runtime->isInstanceOfBytearray(*suffix_obj)) {
    Bytearray suffix(&scope, *suffix_obj);
    suffix_obj = suffix.items();
    suffix_len = suffix.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "endswith first arg must be bytes or a tuple of bytes, not %T",
        &suffix_obj);
  }
  Bytes self(&scope, *self_obj);
  Bytes suffix(&scope, *suffix_obj);
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  Int start(&scope, start_obj.isUnbound() ? Int::cast(SmallInt::fromWord(0))
                                          : intUnderlying(*start_obj));
  Int end(&scope, end_obj.isUnbound() ? Int::cast(SmallInt::fromWord(self_len))
                                      : intUnderlying(*end_obj));
  return runtime->bytesEndsWith(self, self_len, suffix, suffix_len,
                                start.asWordSaturated(), end.asWordSaturated());
}

RawObject FUNC(_builtins, _byteslike_find_byteslike)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  word haystack_len;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes self(&scope, bytesUnderlying(*self_obj));
    self_obj = *self;
    haystack_len = self.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    self_obj = self.items();
    haystack_len = self.numItems();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Object sub_obj(&scope, args.get(1));
  word needle_len;
  if (runtime->isInstanceOfBytes(*sub_obj)) {
    Bytes sub(&scope, bytesUnderlying(*sub_obj));
    sub_obj = *sub;
    needle_len = sub.length();
  } else if (runtime->isInstanceOfBytearray(*sub_obj)) {
    Bytearray sub(&scope, *sub_obj);
    sub_obj = sub.items();
    needle_len = sub.numItems();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Bytes haystack(&scope, *self_obj);
  Bytes needle(&scope, *sub_obj);
  word start = intUnderlying(args.get(2)).asWordSaturated();
  word end = intUnderlying(args.get(3)).asWordSaturated();
  return SmallInt::fromWord(
      bytesFind(haystack, haystack_len, needle, needle_len, start, end));
}

RawObject FUNC(_builtins, _byteslike_find_int)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  word needle = intUnderlying(args.get(1)).asWordSaturated();
  if (needle < 0 || needle > kMaxByte) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  Object self_obj(&scope, args.get(0));
  word start = intUnderlying(args.get(2)).asWordSaturated();
  word end = intUnderlying(args.get(3)).asWordSaturated();
  Bytes haystack(&scope, Bytes::empty());
  word length;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    haystack = bytesUnderlying(*self_obj);
    length = haystack.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    haystack = self.items();
    length = self.numItems();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Slice::adjustSearchIndices(&start, &end, length);
  return SmallInt::fromWord(haystack.findByte(needle, start, end - start));
}

RawObject FUNC(_builtins, _byteslike_guard)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  if (thread->runtime()->isByteslike(*obj)) {
    return NoneType::object();
  }
  return thread->raiseWithFmt(
      LayoutId::kTypeError, "a bytes-like object is required, not '%T'", &obj);
}

RawObject FUNC(_builtins, _byteslike_rfind_byteslike)(Thread* thread,
                                                      Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  word haystack_len;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes self(&scope, bytesUnderlying(*self_obj));
    self_obj = *self;
    haystack_len = self.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    self_obj = self.items();
    haystack_len = self.numItems();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Object sub_obj(&scope, args.get(1));
  word needle_len;
  if (runtime->isInstanceOfBytes(*sub_obj)) {
    Bytes sub(&scope, bytesUnderlying(*sub_obj));
    sub_obj = *sub;
    needle_len = sub.length();
  } else if (runtime->isInstanceOfBytearray(*sub_obj)) {
    Bytearray sub(&scope, *sub_obj);
    sub_obj = sub.items();
    needle_len = sub.numItems();
  } else {
    UNIMPLEMENTED("bytes-like other than bytes, bytearray");
  }
  Bytes haystack(&scope, *self_obj);
  Bytes needle(&scope, *sub_obj);
  word start = intUnderlying(args.get(2)).asWordSaturated();
  word end = intUnderlying(args.get(3)).asWordSaturated();
  return SmallInt::fromWord(
      bytesRFind(haystack, haystack_len, needle, needle_len, start, end));
}

RawObject FUNC(_builtins, _byteslike_rfind_int)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  word sub = intUnderlying(args.get(1)).asWordSaturated();
  if (sub < 0 || sub > kMaxByte) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "byte must be in range(0, 256)");
  }
  Bytes needle(&scope, runtime->newBytes(1, sub));
  Object self_obj(&scope, args.get(0));
  word start = intUnderlying(args.get(2)).asWordSaturated();
  word end = intUnderlying(args.get(3)).asWordSaturated();
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes haystack(&scope, bytesUnderlying(*self_obj));
    return SmallInt::fromWord(bytesRFind(haystack, haystack.length(), needle,
                                         needle.length(), start, end));
  }
  if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    Bytes haystack(&scope, self.items());
    return SmallInt::fromWord(bytesRFind(haystack, self.numItems(), needle,
                                         needle.length(), start, end));
  }
  UNIMPLEMENTED("bytes-like other than bytes, bytearray");
}

RawObject FUNC(_builtins, _byteslike_startswith)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  word self_len;
  if (runtime->isInstanceOfBytes(*self_obj)) {
    Bytes self(&scope, bytesUnderlying(*self_obj));
    self_obj = *self;
    self_len = self.length();
  } else if (runtime->isInstanceOfBytearray(*self_obj)) {
    Bytearray self(&scope, *self_obj);
    self_obj = self.items();
    self_len = self.numItems();
  } else {
    UNREACHABLE("self has an unexpected type");
  }
  DCHECK(self_obj.isBytes(),
         "bytes-like object not resolved to underlying bytes");
  Object prefix_obj(&scope, args.get(1));
  word prefix_len;
  if (runtime->isInstanceOfBytes(*prefix_obj)) {
    Bytes prefix(&scope, bytesUnderlying(*prefix_obj));
    prefix_obj = *prefix;
    prefix_len = prefix.length();
  } else if (runtime->isInstanceOfBytearray(*prefix_obj)) {
    Bytearray prefix(&scope, *prefix_obj);
    prefix_obj = prefix.items();
    prefix_len = prefix.numItems();
  } else {
    // TODO(T38246066): support buffer protocol
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "startswith first arg must be bytes or a tuple of bytes, not %T",
        &prefix_obj);
  }
  Bytes self(&scope, *self_obj);
  Bytes prefix(&scope, *prefix_obj);
  word start = intUnderlying(args.get(2)).asWordSaturated();
  word end = intUnderlying(args.get(3)).asWordSaturated();
  return runtime->bytesStartsWith(self, self_len, prefix, prefix_len, start,
                                  end);
}

RawObject FUNC(_builtins, _caller_function)(Thread* thread, Arguments) {
  return thread->currentFrame()->previousFrame()->previousFrame()->function();
}

RawObject FUNC(_builtins, _caller_locals)(Thread* thread, Arguments) {
  return frameLocals(thread,
                     thread->currentFrame()->previousFrame()->previousFrame());
}

RawObject FUNC(_builtins, _classmethod)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  ClassMethod result(&scope, thread->runtime()->newClassMethod());
  result.setFunction(args.get(0));
  return *result;
}

static RawObject isAbstract(Thread* thread, const Object& obj) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  // TODO(T47800709): make this lookup more efficient
  Object abstract(
      &scope, runtime->attributeAtById(thread, obj, ID(__isabstractmethod__)));
  if (abstract.isError()) {
    Object given(&scope, thread->pendingExceptionType());
    Object exc(&scope, runtime->typeAt(LayoutId::kAttributeError));
    if (givenExceptionMatches(thread, given, exc)) {
      thread->clearPendingException();
      return Bool::falseObj();
    }
    return *abstract;
  }
  return Interpreter::isTrue(thread, *abstract);
}

RawObject FUNC(_builtins, _classmethod_isabstract)(Thread* thread,
                                                   Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfClassMethod(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(classmethod));
  }
  ClassMethod self(&scope, *self_obj);
  Object func(&scope, self.function());
  return isAbstract(thread, func);
}

RawObject FUNC(_builtins, _code_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isCode());
}

RawObject FUNC(_builtins, _code_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isCode()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(code));
}

RawObject FUNC(_builtins, _code_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object cls(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (cls != runtime->typeAt(LayoutId::kCode)) {
    return thread->raiseWithFmt(LayoutId::kTypeError, "require code class");
  }
  word argcount = intUnderlying(args.get(1)).asWord();
  if (argcount < 0) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "argcount must not be negative");
  }
  word posonlyargcount = intUnderlying(args.get(2)).asWord();
  if (posonlyargcount < 0) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "posonlyargcount must not be negative");
  }
  word kwonlyargcount = intUnderlying(args.get(3)).asWord();
  if (kwonlyargcount < 0) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "kwonlyargcount must not be negative");
  }
  word nlocals = intUnderlying(args.get(4)).asWord();
  if (nlocals < 0) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "nlocals must not be negative");
  }

  word stacksize = intUnderlying(args.get(5)).asWord();
  word flags = intUnderlying(args.get(6)).asWord();
  if (argcount < posonlyargcount || stacksize < 0 || flags < 0) {
    return thread->raiseBadInternalCall();
  }

  Object code(&scope, args.get(7));
  Object consts(&scope, args.get(8));
  Object names(&scope, args.get(9));
  Object varnames(&scope, args.get(10));
  Object filename(&scope, args.get(11));
  Object name(&scope, args.get(12));
  word firstlineno = intUnderlying(args.get(13)).asWord();
  Object lnotab(&scope, args.get(14));
  Object freevars(&scope, args.get(15));
  Object cellvars(&scope, args.get(16));
  return runtime->newCode(argcount, posonlyargcount, kwonlyargcount, nlocals,
                          stacksize, flags, code, consts, names, varnames,
                          freevars, cellvars, filename, name, firstlineno,
                          lnotab);
}

RawObject FUNC(_builtins, _code_set_filename)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object code_obj(&scope, args.get(0));
  CHECK(code_obj.isCode(), "Expected code to be a code object");
  Code code(&scope, *code_obj);
  Object filename(&scope, args.get(1));
  CHECK(thread->runtime()->isInstanceOfStr(*filename),
        "Expected value to be a str");
  code.setFilename(*filename);
  return NoneType::object();
}

RawObject FUNC(_builtins, _complex_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfComplex(args.get(0)));
}

RawObject FUNC(_builtins, _complex_checkexact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isComplex());
}

RawObject FUNC(_builtins, _complex_imag)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfComplex(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(complex));
  }
  Complex self(&scope, complexUnderlying(*self_obj));
  return runtime->newFloat(self.imag());
}

static bool unpackNumeric(const Object& val, double* real, double* imag) {
  switch (val.layoutId()) {
    case LayoutId::kBool:
      *real = Bool::cast(*val).value();
      *imag = 0.0;
      return true;
    case LayoutId::kComplex:
      *real = Complex::cast(*val).real();
      *imag = Complex::cast(*val).imag();
      return true;
    case LayoutId::kFloat:
      *real = Float::cast(*val).value();
      *imag = 0.0;
      return true;
    case LayoutId::kSmallInt:
      *real = SmallInt::cast(*val).value();
      *imag = 0.0;
      return true;
    case LayoutId::kUnbound:
      *real = 0.0;
      *imag = 0.0;
      return true;
    default:
      return false;
  }
}

RawObject FUNC(_builtins, _complex_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type cls(&scope, args.get(0));
  DCHECK(cls.builtinBase() == LayoutId::kComplex, "cls must subclass complex");
  Object real_obj(&scope, args.get(1));
  Object imag_obj(&scope, args.get(2));
  if (real_obj.isComplex() && imag_obj.isUnbound() && cls.isBuiltin()) {
    return *real_obj;
  }

  double real1, imag1, real2, imag2;
  if (!unpackNumeric(real_obj, &real1, &imag1) ||
      !unpackNumeric(imag_obj, &real2, &imag2)) {
    return Unbound::object();
  }

  double real = real1 - imag2;
  double imag = imag1 + real2;

  Runtime* runtime = thread->runtime();
  if (cls.isBuiltin()) {
    return runtime->newComplex(real, imag);
  }

  Layout layout(&scope, cls.instanceLayout());
  UserComplexBase result(&scope, runtime->newInstance(layout));
  result.setValue(runtime->newComplex(real, imag));
  return *result;
}

RawObject FUNC(_builtins, _complex_real)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfComplex(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(complex));
  }
  Complex self(&scope, complexUnderlying(*self_obj));
  return runtime->newFloat(self.real());
}

RawObject FUNC(_builtins, _compute_mro)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  return computeMro(thread, type);
}

RawObject FUNC(_builtins, _debug_dump)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object object(&scope, args.get(0));
  std::stringstream stream;
  dumpExtended(stream, *object);
  return thread->runtime()->newStrFromCStr(stream.str().c_str());
}

RawObject FUNC(_builtins, _deque_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfDeque(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(deque));
}

RawObject FUNC(_builtins, _dict_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfDict(args.get(0)));
}

RawObject FUNC(_builtins, _dict_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isDict());
}

RawObject FUNC(_builtins, _dict_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Object key(&scope, args.get(1));
  Object default_obj(&scope, args.get(2));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfDict(*self)) {
    return thread->raiseRequiresType(self, ID(dict));
  }
  Dict dict(&scope, *self);

  // Check key hash
  Object hash_obj(&scope, Interpreter::hash(thread, key));
  if (hash_obj.isErrorException()) return *hash_obj;
  word hash = SmallInt::cast(*hash_obj).value();
  Object result(&scope, dictAt(thread, dict, key, hash));
  if (result.isErrorNotFound()) return *default_obj;
  return *result;
}

RawObject FUNC(_builtins, _dict_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfDict(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(dict));
}

RawObject FUNC(_builtins, _dict_items_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isDictItems()) return NoneType::object();
  return raiseRequiresFromCaller(thread, args, ID(dict_items));
}

RawObject FUNC(_builtins, _dict_keys_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isDictKeys()) return NoneType::object();
  return raiseRequiresFromCaller(thread, args, ID(dict_keys));
}

RawObject FUNC(_builtins, _dict_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Dict self(&scope, args.get(0));
  return SmallInt::fromWord(self.numItems());
}

RawObject FUNC(_builtins, _dict_setitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Object key(&scope, args.get(1));
  Object value(&scope, args.get(2));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfDict(*self)) {
    return thread->raiseRequiresType(self, ID(dict));
  }
  Dict dict(&scope, *self);
  Object hash_obj(&scope, Interpreter::hash(thread, key));
  if (hash_obj.isErrorException()) return *hash_obj;
  word hash = SmallInt::cast(*hash_obj).value();
  Object result(&scope, dictAtPut(thread, dict, key, hash, value));
  if (result.isErrorException()) return *result;
  return NoneType::object();
}

RawObject FUNC(_builtins, _dict_update)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfDict(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(dict));
  }
  Dict self(&scope, *self_obj);
  Object other(&scope, args.get(1));

  if (!other.isUnbound()) {
    RawObject result = dictMergeOverride(thread, self, other);
    if (result.isError()) {
      if (thread->pendingExceptionMatches(LayoutId::kAttributeError)) {
        // no `keys` attribute, bail out to managed code to try tuple unpacking
        thread->clearPendingException();
        return Unbound::object();
      }
      return result;
    }
  }

  Object kwargs(&scope, args.get(2));
  return dictMergeOverride(thread, self, kwargs);
}

RawObject FUNC(_builtins, _divmod)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object number(&scope, args.get(0));
  Object divisor(&scope, args.get(1));
  return Interpreter::binaryOperation(thread, Interpreter::BinaryOp::DIVMOD,
                                      number, divisor);
}

RawObject FUNC(_builtins, _exec)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Code code(&scope, args.get(0));
  Module module(&scope, args.get(1));
  Object implicit_globals(&scope, args.get(2));
  return thread->exec(code, module, implicit_globals);
}

RawObject FUNC(_builtins, _float_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfFloat(args.get(0)));
}

RawObject FUNC(_builtins, _float_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isFloat());
}

static double floatDivmod(double x, double y, double* remainder) {
  double mod = std::fmod(x, y);
  double div = (x - mod) / y;

  if (mod != 0.0) {
    if ((y < 0.0) != (mod < 0.0)) {
      mod += y;
      div -= 1.0;
    }
  } else {
    mod = std::copysign(0.0, y);
  }

  double floordiv = 0;
  if (div != 0.0) {
    floordiv = std::floor(div);
    if (div - floordiv > 0.5) {
      floordiv += 1.0;
    }
  } else {
    floordiv = std::copysign(0.0, x / y);
  }

  *remainder = mod;
  return floordiv;
}

RawObject FUNC(_builtins, _float_divmod)(Thread* thread, Arguments args) {
  HandleScope scope(thread);

  double left = floatUnderlying(args.get(0)).value();
  double divisor = floatUnderlying(args.get(1)).value();
  if (divisor == 0.0) {
    return thread->raiseWithFmt(LayoutId::kZeroDivisionError, "float divmod()");
  }

  double remainder;
  double quotient = floatDivmod(left, divisor, &remainder);
  Runtime* runtime = thread->runtime();
  Object quotient_obj(&scope, runtime->newFloat(quotient));
  Object remainder_obj(&scope, runtime->newFloat(remainder));
  return runtime->newTupleWith2(quotient_obj, remainder_obj);
}

RawObject FUNC(_builtins, _float_format)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  double value = floatUnderlying(args.get(0)).value();
  Str format_code(&scope, args.get(1));
  DCHECK(format_code.length() == 1, "expected len(format_code) == 1");
  char format_code_char = format_code.byteAt(0);
  DCHECK(format_code_char == 'e' || format_code_char == 'E' ||
             format_code_char == 'f' || format_code_char == 'F' ||
             format_code_char == 'g' || format_code_char == 'G' ||
             format_code_char == 'r',
         "expected format_code in 'eEfFgGr'");
  SmallInt precision(&scope, args.get(2));
  Bool always_add_sign(&scope, args.get(3));
  Bool add_dot_0(&scope, args.get(4));
  Bool use_alt_formatting(&scope, args.get(5));
  unique_c_ptr<char> c_str(doubleToString(
      value, format_code_char, precision.value(), always_add_sign.value(),
      add_dot_0.value(), use_alt_formatting.value(), nullptr));
  return thread->runtime()->newStrFromCStr(c_str.get());
}

RawObject FUNC(_builtins, _float_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfFloat(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(float));
}

static RawObject floatNew(Thread* thread, const Type& type, RawObject flt) {
  DCHECK(flt.isFloat(), "unexpected type when creating float");
  if (type.isBuiltin()) return flt;
  HandleScope scope(thread);
  Layout type_layout(&scope, type.instanceLayout());
  UserFloatBase instance(&scope, thread->runtime()->newInstance(type_layout));
  instance.setValue(flt);
  return *instance;
}

static RawObject floatNewFromDigits(Thread* thread, const Type& type,
                                    const char* str, word length) {
  const char* last = str + length - 1;
  // strip spaces
  while (str < last && ASCII::isSpace(*str)) {
    str++;
  }
  while (last > str && ASCII::isSpace(*last)) {
    last--;
  }

  RawObject float_obj = floatFromDigits(thread, str, last - str + 1);
  if (float_obj.isErrorException()) {
    return float_obj;
  }

  return floatNew(thread, type, float_obj);
}

RawObject FUNC(_builtins, _float_new_from_byteslike)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Object arg(&scope, args.get(1));
  Runtime* runtime = thread->runtime();
  Bytes underlying(&scope, Bytes::empty());
  word length;
  if (runtime->isInstanceOfBytes(*arg)) {
    underlying = bytesUnderlying(*arg);
    length = underlying.length();
  } else {
    // TODO(T57022841): follow full CPython conversion for bytes-like objects
    UNIMPLEMENTED("float.__new__ from byteslike");
  }
  unique_c_ptr<byte> c_str(reinterpret_cast<byte*>(std::malloc(length + 1)));
  c_str.get()[length] = '\0';
  underlying.copyTo(c_str.get(), length);
  return floatNewFromDigits(thread, type, reinterpret_cast<char*>(c_str.get()),
                            length);
}

RawObject FUNC(_builtins, _float_new_from_float)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  return floatNew(thread, type, args.get(1));
}

RawObject FUNC(_builtins, _float_new_from_str)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Object arg(&scope, args.get(1));
  Str str(&scope, strUnderlying(*arg));
  unique_c_ptr<char> c_str(str.toCStr());
  word length = str.length();
  return floatNewFromDigits(thread, type, c_str.get(), length);
}

RawObject FUNC(_builtins, _float_signbit)(Thread*, Arguments args) {
  double value = floatUnderlying(args.get(0)).value();
  return Bool::fromBool(std::signbit(value));
}

RawObject FUNC(_builtins, _frozenset_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfFrozenSet(args.get(0)));
}

RawObject FUNC(_builtins, _frozenset_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfFrozenSet(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(frozenset));
}

RawObject FUNC(_builtins, _function_annotations)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  Object annotations(&scope, function.annotations());
  if (annotations.isNoneType()) {
    annotations = thread->runtime()->newDict();
    function.setAnnotations(*annotations);
  }
  return *annotations;
}

RawObject FUNC(_builtins, _function_closure)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  return function.closure();
}

RawObject FUNC(_builtins, _function_defaults)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  return function.defaults();
}

RawObject FUNC(_builtins, _function_globals)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  // extension functions created via C-API have no associated module.
  if (function.moduleObject().isNoneType()) {
    return thread->runtime()->newDict();
  }
  Module module(&scope, function.moduleObject());
  return module.moduleProxy();
}

RawObject FUNC(_builtins, _function_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isFunction()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(function));
}

RawObject FUNC(_builtins, _function_kwdefaults)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  return function.kwDefaults();
}

RawObject FUNC(_builtins, _function_lineno)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Function function(&scope, args.get(0));
  SmallInt pc(&scope, args.get(1));
  Code code(&scope, function.code());
  return SmallInt::fromWord(code.offsetToLineNum(pc.value()));
}

RawObject FUNC(_builtins, _function_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object cls_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*cls_obj)) {
    return thread->raiseRequiresType(cls_obj, ID(function));
  }
  Type cls(&scope, *cls_obj);
  if (cls.builtinBase() != LayoutId::kFunction) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "not a subtype of function");
  }
  Object code_obj(&scope, args.get(1));
  if (!code_obj.isCode()) {
    return thread->raiseRequiresType(code_obj, ID(code));
  }
  Object module(&scope, args.get(2));
  if (!runtime->isInstanceOfModule(*module)) {
    return thread->raiseRequiresType(module, ID(module));
  }
  Code code(&scope, *code_obj);
  Object empty_qualname(&scope, NoneType::object());
  Object result(&scope, runtime->newFunctionWithCode(thread, empty_qualname,
                                                     code, module));
  if (result.isFunction()) {
    Function new_function(&scope, *result);

    Object name(&scope, args.get(3));
    if (runtime->isInstanceOfStr(*name)) {
      new_function.setName(*name);
    } else if (!name.isNoneType()) {
      return thread->raiseWithFmt(LayoutId::kTypeError,
                                  "arg 3 (name) must be None or string", &name);
    }
    Object defaults(&scope, args.get(4));
    if (runtime->isInstanceOfTuple(*defaults)) {
      new_function.setDefaults(*defaults);
    } else if (!defaults.isNoneType()) {
      return thread->raiseWithFmt(LayoutId::kTypeError,
                                  "arg 4 (defaults) must be None or tuple",
                                  &defaults);
    }
    Object closure(&scope, args.get(5));
    if (runtime->isInstanceOfTuple(*closure)) {
      new_function.setClosure(*closure);
    } else if (!closure.isNoneType()) {
      return thread->raiseWithFmt(LayoutId::kTypeError,
                                  "arg 5 (closure) must be None or tuple",
                                  &closure);
    }
    return *new_function;
  }
  return *result;
}

RawObject FUNC(_builtins, _function_set_annotations)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  Object annotations(&scope, args.get(1));
  if (thread->runtime()->isInstanceOfDict(*annotations) ||
      annotations.isNoneType()) {
    function.setAnnotations(*annotations);
    return NoneType::object();
  }
  return thread->raiseRequiresType(annotations, ID(dict));
}

RawObject FUNC(_builtins, _function_set_defaults)(Thread* thread,
                                                  Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  Object defaults(&scope, args.get(1));
  if (defaults.isNoneType()) {
    function.setDefaults(*defaults);
    return NoneType::object();
  }
  if (thread->runtime()->isInstanceOfTuple(*defaults)) {
    function.setDefaults(tupleUnderlying(*defaults));
    return NoneType::object();
  }
  return thread->raiseRequiresType(defaults, ID(tuple));
}

RawObject FUNC(_builtins, _function_set_kwdefaults)(Thread* thread,
                                                    Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  if (!self.isFunction()) {
    return thread->raiseRequiresType(self, ID(function));
  }
  Function function(&scope, *self);
  Object kwdefaults(&scope, args.get(1));
  if (thread->runtime()->isInstanceOfDict(*kwdefaults) ||
      kwdefaults.isNoneType()) {
    function.setKwDefaults(*kwdefaults);
    return NoneType::object();
  }
  return thread->raiseRequiresType(kwdefaults, ID(dict));
}

RawObject FUNC(_builtins, _gc)(Thread* thread, Arguments) {
  thread->runtime()->collectGarbage();
  return NoneType::object();
}

RawObject FUNC(_builtins, _get_asyncgen_hooks)(Thread* thread, Arguments) {
  HandleScope scope(thread);
  Object firstiter(&scope, thread->asyncgenHooksFirstIter());
  Object finalizer(&scope, thread->asyncgenHooksFinalizer());
  return thread->runtime()->newTupleWith2(firstiter, finalizer);
}

RawObject FUNC(_builtins, _get_member_byte)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  char value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), 1);
  return thread->runtime()->newInt(value);
}

RawObject FUNC(_builtins, _get_member_char)(Thread*, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  return SmallStr::fromCodePoint(*reinterpret_cast<byte*>(addr));
}

RawObject FUNC(_builtins, _get_member_double)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  double value = 0.0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newFloat(value);
}

RawObject FUNC(_builtins, _get_member_float)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  float value = 0.0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newFloat(value);
}

RawObject FUNC(_builtins, _get_member_int)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  int value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newInt(value);
}

RawObject FUNC(_builtins, _get_member_long)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  long value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newInt(value);
}

RawObject FUNC(_builtins, _get_member_pyobject)(Thread* thread,
                                                Arguments args) {
  return objectGetMember(thread, args.get(0), args.get(1));
}

RawObject FUNC(_builtins, _get_member_short)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  short value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newInt(value);
}

RawObject FUNC(_builtins, _get_member_string)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  if (*reinterpret_cast<char**>(addr) == nullptr) return NoneType::object();
  return thread->runtime()->newStrFromCStr(*reinterpret_cast<char**>(addr));
}

RawObject FUNC(_builtins, _get_member_ubyte)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  unsigned char value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newIntFromUnsigned(value);
}

RawObject FUNC(_builtins, _get_member_uint)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  unsigned int value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newIntFromUnsigned(value);
}

RawObject FUNC(_builtins, _get_member_ulong)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  unsigned long value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newIntFromUnsigned(value);
}

RawObject FUNC(_builtins, _get_member_ushort)(Thread* thread, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  unsigned short value = 0;
  std::memcpy(&value, reinterpret_cast<void*>(addr), sizeof(value));
  return thread->runtime()->newIntFromUnsigned(value);
}

RawObject FUNC(_builtins, _heap_dump)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str filename(&scope, args.get(0));
  unique_c_ptr<char> filename_str(filename.toCStr());
  return heapDump(thread, filename_str.get());
}

RawObject FUNC(_builtins, _instance_dunder_dict_set)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Instance instance(&scope, args.get(0));
  Object dict_obj(&scope, args.get(1));
  Runtime* runtime = thread->runtime();
  if (!dict_obj.isDict()) {
    if (runtime->isInstanceOfDict(*dict_obj)) {
      // TODO(T64971317): Support dict subclass.
      UNIMPLEMENTED("dict subclass is not supported yet.");
    }
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "__dict__ must be set to a dictionary, "
                                "not a '%T'",
                                &dict_obj);
  }
  Layout layout(&scope, runtime->layoutAt(instance.layoutId()));
  // Set in-object attribute values to None.
  Tuple in_object(&scope, layout.inObjectAttributes());
  word num_in_object_attr = in_object.length();
  for (word i = 0; i < num_in_object_attr; i++) {
    Tuple entry(&scope, in_object.at(i));
    AttributeInfo info(entry.at(1));
    instance.instanceVariableAtPut(info.offset(), NoneType::object());
  }
  Type type(&scope, layout.describedType());
  if (type.isBuiltin()) {
    // TODO(T65043421): Support builtin type.
    UNIMPLEMENTED("_instance_dunder_dict_set(builtin_type_object)");
  }
  Layout new_layout(
      &scope, runtime->typeDictOnlyLayout(thread, type, num_in_object_attr));
  DCHECK(new_layout.hasDictOverflow(), "dict overflow is expected");
  instance.setLayoutId(new_layout.id());
  instance.instanceVariableAtPut(new_layout.dictOverflowOffset(), *dict_obj);
  return NoneType::object();
}

RawObject FUNC(_builtins, _instance_delattr)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Instance instance(&scope, args.get(0));
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  return instanceDelAttr(thread, instance, name);
}

RawObject FUNC(_builtins, _instance_getattr)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Instance instance(&scope, args.get(0));
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object result(&scope, instanceGetAttribute(thread, instance, name));
  return result.isErrorNotFound() ? Unbound::object() : *result;
}

RawObject FUNC(_builtins, _instance_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isInstance()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(instance));
}

RawObject FUNC(_builtins, _instance_overflow_dict)(Thread* thread,
                                                   Arguments args) {
  HandleScope scope(thread);
  Object object(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  Layout layout(&scope, runtime->layoutOf(*object));
  CHECK(layout.hasDictOverflow(), "expected dict overflow layout");
  word offset = layout.dictOverflowOffset();
  Instance instance(&scope, *object);
  Object overflow_dict_obj(&scope, instance.instanceVariableAt(offset));
  if (overflow_dict_obj.isNoneType()) {
    overflow_dict_obj = runtime->newDict();
    instance.instanceVariableAtPut(offset, *overflow_dict_obj);
  }
  return *overflow_dict_obj;
}

RawObject FUNC(_builtins, _instance_setattr)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Instance instance(&scope, args.get(0));
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object value(&scope, args.get(2));
  return instanceSetAttr(thread, instance, name, value);
}

RawObject FUNC(_builtins, _int_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfInt(args.get(0)));
}

RawObject FUNC(_builtins, _int_check_exact)(Thread*, Arguments args) {
  RawObject arg = args.get(0);
  return Bool::fromBool(arg.isSmallInt() || arg.isLargeInt());
}

RawObject FUNC(_builtins, _instancemethod_func)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  if (!obj.isInstanceMethod()) {
    return thread->raiseRequiresType(obj, ID(instancemethod));
  }
  return InstanceMethod::cast(*obj).function();
}

static RawObject positiveIntFromSmallStrWithBase10(RawSmallStr str) {
  word length = str.length();
  if (length == 0) {
    return NoneType::object();
  }
  word result = 0;
  for (word i = 0; i < length; i++) {
    byte b = str.byteAt(i);
    if ('0' <= b && b <= '9') {
      result *= 10;
      result += b - '0';
    } else {
      return NoneType::object();
    }
  }
  return SmallInt::fromWord(result);
}

RawObject FUNC(_builtins, _int_ctor)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kInt), "unexpected cls");
  RawObject x_raw = args.get(1);
  RawObject base_raw = args.get(2);
  LayoutId x_layout_id = x_raw.layoutId();
  if (base_raw.isUnbound()) {
    switch (x_layout_id) {
      case LayoutId::kSmallInt:
        return x_raw;
      case LayoutId::kBool:
        return SmallInt::fromWord(Bool::cast(x_raw).value());
      case LayoutId::kFloat:
        return intFromDouble(thread, Float::cast(x_raw).value());
      case LayoutId::kSmallStr: {
        RawObject result =
            positiveIntFromSmallStrWithBase10(SmallStr::cast(x_raw));
        if (!result.isNoneType()) {
          return result;
        }
        break;
      }
      case LayoutId::kUnbound:
        return SmallInt::fromWord(0);
      default:
        break;
    }
  }
  HandleScope scope(thread);
  Type cls(&scope, args.get(0));
  Object x(&scope, x_raw);
  Object base(&scope, base_raw);
  return thread->invokeFunction3(ID(_builtins), ID(_type_dunder_call), cls, x,
                                 base);
}

RawObject FUNC(_builtins, _int_ctor_obj)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kInt), "unexpected cls");
  RawObject x_raw = args.get(1);
  LayoutId x_layout_id = x_raw.layoutId();
  switch (x_layout_id) {
    case LayoutId::kSmallInt:
      return x_raw;
    case LayoutId::kBool:
      return SmallInt::fromWord(Bool::cast(x_raw).value());
    case LayoutId::kFloat:
      return intFromDouble(thread, Float::cast(x_raw).value());
    case LayoutId::kSmallStr: {
      RawObject result =
          positiveIntFromSmallStrWithBase10(SmallStr::cast(x_raw));
      if (!result.isNoneType()) {
        return result;
      }
      break;
    }
    default:
      break;
  }
  HandleScope scope(thread);
  Type cls(&scope, args.get(0));
  Object x(&scope, x_raw);
  Object base(&scope, Unbound::object());
  return thread->invokeFunction3(ID(_builtins), ID(_type_dunder_call), cls, x,
                                 base);
}

static RawObject intOrUserSubclass(Thread* thread, const Type& type,
                                   const Object& value) {
  DCHECK(value.isSmallInt() || value.isLargeInt(),
         "builtin value should have type int");
  DCHECK(type.builtinBase() == LayoutId::kInt, "type must subclass int");
  if (type.isBuiltin()) return *value;
  HandleScope scope(thread);
  Layout layout(&scope, type.instanceLayout());
  UserIntBase instance(&scope, thread->runtime()->newInstance(layout));
  instance.setValue(*value);
  return *instance;
}

RawObject FUNC(_builtins, _int_from_bytes)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();

  Type type(&scope, args.get(0));
  Bytes bytes(&scope, args.get(1));
  Bool byteorder_big(&scope, args.get(2));
  endian endianness = byteorder_big.value() ? endian::big : endian::little;
  Bool signed_arg(&scope, args.get(3));
  bool is_signed = signed_arg == Bool::trueObj();
  Int value(&scope, runtime->bytesToInt(thread, bytes, endianness, is_signed));
  return intOrUserSubclass(thread, type, value);
}

RawObject FUNC(_builtins, _int_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfInt(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(int));
}

static word digitValue(byte digit, word base) {
  if ('0' <= digit && digit < '0' + base) return digit - '0';
  // Bases 2-10 are limited to numerals, but all greater bases can use letters
  // too.
  if (base <= 10) return -1;
  if ('a' <= digit && digit < 'a' + base - 10) return digit - 'a' + 10;
  if ('A' <= digit && digit < 'A' + base - 10) return digit - 'A' + 10;
  return -1;
}

static word inferBase(byte second_byte) {
  switch (second_byte) {
    case 'x':
    case 'X':
      return 16;
    case 'o':
    case 'O':
      return 8;
    case 'b':
    case 'B':
      return 2;
    default:
      return 10;
  }
}

static RawObject intFromByteslike(Thread* thread, const Byteslike& byteslike,
                                  word length, word base) {
  DCHECK_BOUND(length, byteslike.length());
  DCHECK(base == 0 || (base >= 2 && base <= 36), "invalid base");
  if (length <= 0) return Error::error();

  // Clamp the length at the last whitespace character.
  word idx = length;
  byte b = byteslike.byteAt(--idx);
  while (ASCII::isSpace(b)) {
    if (idx <= 0) return Error::error();
    b = byteslike.byteAt(--idx);
  }
  length = idx + 1;

  // Start the index from the first non-zero whitespace character.
  idx = 0;
  if (idx >= length) return Error::error();
  b = byteslike.byteAt(idx++);
  while (ASCII::isSpace(b)) {
    if (idx >= length) return Error::error();
    b = byteslike.byteAt(idx++);
  }
  word sign = 1;
  switch (b) {
    case '-':
      sign = -1;
      // fall through
    case '+':
      if (idx >= length) return Error::error();
      b = byteslike.byteAt(idx++);
      break;
  }

  word inferred_base = 10;
  if (b == '0') {
    if (idx >= length) return SmallInt::fromWord(0);
    inferred_base = inferBase(byteslike.byteAt(idx));
    if (base == 0) base = inferred_base;
    if (inferred_base != 10 && base == inferred_base) {
      if (++idx >= length) return Error::error();
      b = byteslike.byteAt(idx++);
    }
  } else if (base == 0) {
    base = 10;
  }

  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Int result(&scope, SmallInt::fromWord(0));
  Int digit(&scope, SmallInt::fromWord(0));
  Int base_obj(&scope, SmallInt::fromWord(base));
  word num_start = idx;
  for (;;) {
    if (b == '_') {
      // No leading underscores unless the number has a prefix
      if (idx == num_start && inferred_base == 10) return Error::error();
      // No trailing underscores
      if (idx >= length) return Error::error();
      b = byteslike.byteAt(idx++);
    }
    word digit_val = digitValue(b, base);
    if (digit_val == -1) return Error::error();
    digit = Int::cast(SmallInt::fromWord(digit_val));
    result = runtime->intAdd(thread, result, digit);
    if (idx >= length) break;
    b = byteslike.byteAt(idx++);
    result = runtime->intMultiply(thread, result, base_obj);
  }
  if (sign < 0) {
    result = runtime->intNegate(thread, result);
  }
  return *result;
}

RawObject FUNC(_builtins, _int_new_from_byteslike)(Thread* thread,
                                                   Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Byteslike byteslike(&scope, thread, args.get(1));
  word base = intUnderlying(args.get(2)).asWord();
  Object result(&scope,
                intFromByteslike(thread, byteslike, byteslike.length(), base));
  if (result.isError()) {
    Str repr(&scope, byteslikeReprSmartQuotes(thread, byteslike));
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "invalid literal for int() with base %w: %S",
                                base, &repr);
  }
  return intOrUserSubclass(thread, type, result);
}

RawObject FUNC(_builtins, _int_new_from_int)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Object value(&scope, args.get(1));
  if (value.isBool()) {
    value = convertBoolToInt(*value);
  } else if (!value.isSmallInt() && !value.isLargeInt()) {
    value = intUnderlying(*value);
  }
  return intOrUserSubclass(thread, type, value);
}

static RawObject intFromStr(Thread* thread, const Str& str, word base) {
  DCHECK(base == 0 || (base >= 2 && base <= 36), "invalid base");
  // CPython allows leading whitespace in the integer literal
  word start = strFindFirstNonWhitespace(str);
  if (str.length() - start == 0) {
    return Error::error();
  }
  word sign = 1;
  if (str.byteAt(start) == '-') {
    sign = -1;
    start += 1;
  } else if (str.byteAt(start) == '+') {
    start += 1;
  }
  if (str.length() - start == 0) {
    // Just the sign
    return Error::error();
  }
  if (str.length() - start == 1) {
    // Single digit, potentially with +/-
    word result = digitValue(str.byteAt(start), base == 0 ? 10 : base);
    if (result == -1) return Error::error();
    return SmallInt::fromWord(sign * result);
  }
  // Decimal literals start at the index 0 (no prefix).
  // Octal literals (0oFOO), hex literals (0xFOO), and binary literals (0bFOO)
  // start at index 2.
  word inferred_base = 10;
  if (str.byteAt(start) == '0' && start + 1 < str.length()) {
    inferred_base = inferBase(str.byteAt(start + 1));
  }
  if (base == 0) {
    base = inferred_base;
  }
  if (base == 2 || base == 8 || base == 16) {
    if (base == inferred_base) {
      // This handles integer literals with a base prefix, e.g.
      // * int("0b1", 0) => 1, where the base is inferred from the prefix
      // * int("0b1", 2) => 1, where the prefix matches the provided base
      //
      // If the prefix does not match the provided base, then we treat it as
      // part as part of the number, e.g.
      // * int("0b1", 10) => ValueError
      // * int("0b1", 16) => 177
      start += 2;
    }
    if (str.length() - start == 0) {
      // Just the prefix: 0x, 0b, 0o, etc
      return Error::error();
    }
  }
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Int result(&scope, SmallInt::fromWord(0));
  Int digit(&scope, SmallInt::fromWord(0));
  Int base_obj(&scope, SmallInt::fromWord(base));
  for (word i = start; i < str.length(); i++) {
    byte digit_char = str.byteAt(i);
    if (digit_char == '_') {
      // No leading underscores unless the number has a prefix
      if (i == start && inferred_base == 10) return Error::error();
      // No trailing underscores
      if (i + 1 == str.length()) return Error::error();
      digit_char = str.byteAt(++i);
    }
    word digit_val = digitValue(digit_char, base);
    if (digit_val == -1) return Error::error();
    digit = Int::cast(SmallInt::fromWord(digit_val));
    result = runtime->intMultiply(thread, result, base_obj);
    result = runtime->intAdd(thread, result, digit);
  }
  if (sign < 0) {
    result = runtime->intNegate(thread, result);
  }
  return *result;
}

RawObject FUNC(_builtins, _int_new_from_str)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Str str(&scope, args.get(1));
  word base = intUnderlying(args.get(2)).asWord();
  Object result(&scope, intFromStr(thread, str, base));
  if (result.isError()) {
    Str repr(&scope, thread->invokeMethod1(str, ID(__repr__)));
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "invalid literal for int() with base %w: %S",
                                base == 0 ? 10 : base, &repr);
  }
  return intOrUserSubclass(thread, type, result);
}

RawObject FUNC(_builtins, _iter)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object object(&scope, args.get(0));
  return Interpreter::createIterator(thread, object);
}

static RawObject unpackFunction(const Object& obj) {
  if (obj.isStaticMethod()) {
    return StaticMethod::cast(*obj).function();
  }
  if (obj.isClassMethod()) {
    return ClassMethod::cast(*obj).function();
  }
  if (obj.isBoundMethod()) {
    return BoundMethod::cast(*obj).function();
  }
  if (obj.isInstanceMethod()) {
    return InstanceMethod::cast(*obj).function();
  }
  return *obj;
}

RawObject FUNC(_builtins, _jit)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  obj = unpackFunction(obj);
  if (!obj.isFunction()) {
    // TODO(T90869918): Support unpacking property (fget, fset, fdel).
    return Bool::falseObj();
  }
  Function function(&scope, *obj);
  if (!canCompileFunction(thread, function)) {
    return Bool::falseObj();
  }
  compileFunction(thread, function);
  return Bool::trueObj();
}

RawObject FUNC(_builtins, _jit_iscompiled)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  obj = unpackFunction(obj);
  if (!obj.isFunction()) {
    // TODO(T90869918): Support unpacking property (fget, fset, fdel).
    return Bool::falseObj();
  }
  Function function(&scope, *obj);
  return Bool::fromBool(function.isCompiled());
}

RawObject FUNC(_builtins, _list_append)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfList(*self)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "'append' for 'list' objects doesn't apply to a '%T' object", &self);
  }
  List list(&scope, *self);
  Object value(&scope, args.get(1));
  runtime->listAdd(thread, list, value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _list_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfList(args.get(0)));
}

RawObject FUNC(_builtins, _list_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isList());
}

RawObject FUNC(_builtins, _list_delitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  List self(&scope, args.get(0));
  word length = self.numItems();
  word idx = intUnderlying(args.get(1)).asWordSaturated();
  if (idx < 0) {
    idx += length;
  }
  if (idx < 0 || idx >= length) {
    return thread->raiseWithFmt(LayoutId::kIndexError,
                                "list assignment index out of range");
  }
  listPop(thread, self, idx);
  return NoneType::object();
}

RawObject FUNC(_builtins, _list_delslice)(Thread* thread, Arguments args) {
  // This function deletes elements that are specified by a slice by copying.
  // It compacts to the left elements in the slice range and then copies
  // elements after the slice into the free area.  The list element count is
  // decremented and elements in the unused part of the list are overwritten
  // with None.
  HandleScope scope(thread);
  List list(&scope, args.get(0));

  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();

  word slice_length = Slice::length(start, stop, step);
  DCHECK(slice_length >= 0, "slice length should be positive");
  if (slice_length == 0) {
    // Nothing to delete
    return NoneType::object();
  }
  if (slice_length == list.numItems()) {
    // Delete all the items
    list.clearFrom(0);
    return NoneType::object();
  }
  if (step < 0) {
    // Adjust step to make iterating easier
    start = start + step * (slice_length - 1);
    step = -step;
  }
  DCHECK(start >= 0, "start should be positive");
  DCHECK(start < list.numItems(), "start should be in bounds");
  DCHECK(step <= list.numItems() || slice_length == 1,
         "Step should be in bounds or only one element should be sliced");
  // Sliding compaction of elements out of the slice to the left
  // Invariant: At each iteration of the loop, `fast` is the index of an
  // element addressed by the slice.
  // Invariant: At each iteration of the inner loop, `slow` is the index of a
  // location to where we are relocating a slice addressed element. It is *not*
  // addressed by the slice.
  word fast = start;
  for (word i = 1; i < slice_length; i++) {
    DCHECK_INDEX(fast, list.numItems());
    word slow = fast + 1;
    fast += step;
    for (; slow < fast; slow++) {
      list.atPut(slow - i, list.at(slow));
    }
  }
  // Copy elements into the space where the deleted elements were
  for (word i = fast + 1; i < list.numItems(); i++) {
    list.atPut(i - slice_length, list.at(i));
  }
  word new_length = list.numItems() - slice_length;
  DCHECK(new_length >= 0, "new_length must be positive");
  // Untrack all deleted elements
  list.clearFrom(new_length);
  return NoneType::object();
}

RawObject FUNC(_builtins, _list_extend)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfList(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(list));
  }
  List self(&scope, *self_obj);
  Object other(&scope, args.get(1));

  word src_length;
  Tuple src(&scope, runtime->emptyTuple());
  if (other.isList()) {
    src = List::cast(*other).items();
    src_length = List::cast(*other).numItems();
  } else if (other.isTuple()) {
    src = *other;
    src_length = src.length();
  } else {
    return Unbound::object();
  }

  listExtend(thread, self, src, src_length);
  return NoneType::object();
}

RawObject FUNC(_builtins, _list_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfList(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(list));
  }
  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    word index = intUnderlying(*key).asWordSaturated();
    if (!SmallInt::isValid(index)) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }
    List self(&scope, *self_obj);
    word length = self.numItems();
    if (index < 0) {
      index += length;
    }
    if (index < 0 || index >= length) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "list index out of range");
    }
    return self.at(index);
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  List self(&scope, *self_obj);
  word result_len = Slice::adjustIndices(self.numItems(), &start, &stop, 1);
  if (result_len == 0) {
    return runtime->newList();
  }
  Tuple src(&scope, self.items());
  MutableTuple dst(&scope, runtime->newMutableTuple(result_len));
  dst.replaceFromWithStartAt(0, *src, result_len, start);
  List result(&scope, runtime->newList());
  result.setItems(*dst);
  result.setNumItems(result_len);
  return *result;
}

RawObject FUNC(_builtins, _list_getslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  List self(&scope, args.get(0));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  return listSlice(thread, self, start, stop, step);
}

RawObject FUNC(_builtins, _list_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfList(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(list));
}

RawObject FUNC(_builtins, _list_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  List self(&scope, args.get(0));
  return SmallInt::fromWord(self.numItems());
}

RawObject FUNC(_builtins, _list_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  word size = SmallInt::cast(args.get(0)).value();
  Runtime* runtime = thread->runtime();
  List result(&scope, runtime->newList());
  if (size > 0) {
    MutableTuple items(&scope, runtime->newMutableTuple(size));
    result.setItems(*items);
    result.setNumItems(size);
    items.fill(args.get(1));
  }
  return *result;
}

RawObject FUNC(_builtins, _list_sort)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  CHECK(thread->runtime()->isInstanceOfList(args.get(0)),
        "Unsupported argument type for 'ls'");
  List list(&scope, args.get(0));
  return listSort(thread, list);
}

RawObject FUNC(_builtins, _list_sort_by_key)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  CHECK(thread->runtime()->isInstanceOfList(args.get(0)),
        "Unsupported argument type for 'ls'");
  List list(&scope, args.get(0));
  return listSortWithCompareMethod(thread, list, ID(_lt_key));
}

static RawObject listSetSlice(Thread* thread, const List& self, word start,
                              word stop, word step, const Tuple& src,
                              word src_length) {
  // Make sure that the degenerate case of a slice assignment where start is
  // greater than stop inserts before the start and not the stop. For example,
  // b[5:2] = ... should inserts before 5, not before 2.
  if ((step < 0 && start < stop) || (step > 0 && start > stop)) {
    stop = start;
  }

  if (step == 1) {
    word growth = src_length - (stop - start);
    word new_length = self.numItems() + growth;
    if (growth == 0) {
      // Assignment does not change the length of the list. Do nothing.
    } else if (growth > 0) {
      // Assignment grows the length of the list. Ensure there is enough free
      // space in the underlying tuple for the new items and move stuff out of
      // the way.
      thread->runtime()->listEnsureCapacity(thread, self, new_length);
      // Make the free space part of the list. Must happen before shifting so
      // we can index into the free space.
      self.setNumItems(new_length);
      // Shift some items to the right.
      self.replaceFromWithStartAt(start + growth, *self,
                                  new_length - growth - start, start);
    } else {
      // Growth is negative so assignment shrinks the length of the list.
      // Shift some items to the left.
      self.replaceFromWithStartAt(start, *self, new_length - start,
                                  start - growth);
      // Do not retain references in the unused part of the list.
      self.clearFrom(new_length);
      // Remove the free space from the length of the list. Must happen after
      // shifting and clearing so we can index into the free space.
      self.setNumItems(new_length);
    }

    // Copy new elements into the middle
    if (new_length > 0) {
      MutableTuple::cast(self.items()).replaceFromWith(start, *src, src_length);
    }
    return NoneType::object();
  }

  word slice_length = Slice::length(start, stop, step);
  if (slice_length != src_length) {
    return thread->raiseWithFmt(
        LayoutId::kValueError,
        "attempt to assign sequence of size %w to extended slice of size "
        "%w",
        src_length, slice_length);
  }
  HandleScope scope(thread);
  MutableTuple dst_items(&scope, self.items());
  for (word dst_idx = start, src_idx = 0; src_idx < src_length;
       dst_idx += step, src_idx++) {
    dst_items.atPut(dst_idx, src.at(src_idx));
  }
  return NoneType::object();
}

RawObject FUNC(_builtins, _list_setitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfList(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(list));
  }
  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    word index = intUnderlying(*key).asWordSaturated();
    if (!SmallInt::isValid(index)) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }

    List self(&scope, *self_obj);
    word length = self.numItems();
    if (index < 0) {
      index += length;
    }
    if (index < 0 || index >= length) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "list assignment index out of range");
    }

    self.atPut(index, args.get(2));
    return NoneType::object();
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  Object src(&scope, args.get(2));
  Tuple src_tuple(&scope, runtime->emptyTuple());
  word src_length;
  if (src.isList()) {
    if (self_obj == src) {
      return Unbound::object();
    }
    RawList src_list = List::cast(*src);
    src_tuple = src_list.items();
    src_length = src_list.numItems();
  } else if (src.isTuple()) {
    src_tuple = *src;
    src_length = src_tuple.length();
  } else {
    return Unbound::object();
  }

  List self(&scope, *self_obj);
  Slice::adjustIndices(self.numItems(), &start, &stop, 1);
  return listSetSlice(thread, self, start, stop, 1, src_tuple, src_length);
}

RawObject FUNC(_builtins, _list_setslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();

  List self(&scope, args.get(0));
  Object src(&scope, args.get(4));
  Tuple src_tuple(&scope, runtime->emptyTuple());
  word src_length;
  if (src.isList()) {
    RawList src_list = List::cast(*src);
    src_tuple = src_list.items();
    src_length = src_list.numItems();
    if (self == src) {
      // This copy avoids complicated indexing logic in a rare case of
      // replacing lhs with elements of rhs when lhs == rhs. It can likely be
      // re-written to avoid allocation if necessary.
      src_tuple = runtime->tupleSubseq(thread, src_tuple, 0, src_length);
    }
  } else if (src.isTuple()) {
    src_tuple = *src;
    src_length = src_tuple.length();
  } else {
    return Unbound::object();
  }

  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  return listSetSlice(thread, self, start, stop, step, src_tuple, src_length);
}

RawObject FUNC(_builtins, _list_swap)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  List list(&scope, args.get(0));
  word i = SmallInt::cast(args.get(1)).value();
  word j = SmallInt::cast(args.get(2)).value();
  list.swap(i, j);
  return NoneType::object();
}

RawObject FUNC(_builtins, _memoryview_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return raiseRequiresFromCaller(thread, args, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);

  Object key_obj(&scope, args.get(1));
  if (!thread->runtime()->isInstanceOfInt(*key_obj)) {
    return Unbound::object();
  }
  word index = intUnderlying(*key_obj).asWordSaturated();
  if (!SmallInt::isValid(index)) {
    return thread->raiseWithFmt(LayoutId::kIndexError,
                                "cannot fit '%T' into an index-sized integer",
                                &key_obj);
  }
  word index_abs = std::abs(index);
  word length = self.length();
  word item_size = memoryviewItemsize(thread, self);
  word byte_index;
  if (__builtin_mul_overflow(index_abs, item_size, &byte_index) ||
      length == 0) {
    return thread->raiseWithFmt(LayoutId::kIndexError, "index out of bounds");
  }
  if (index < 0) {
    byte_index = length - byte_index;
  }
  if (byte_index + (item_size - 1) >= length) {
    return thread->raiseWithFmt(LayoutId::kIndexError, "index out of bounds");
  }
  return memoryviewGetitem(thread, self, byte_index);
}

RawObject FUNC(_builtins, _memoryview_getslice)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return thread->raiseRequiresType(self_obj, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);
  Int start_int(&scope, intUnderlying(args.get(1)));
  word start = start_int.asWord();
  Int stop_int(&scope, intUnderlying(args.get(2)));
  word stop = stop_int.asWord();
  Int step_int(&scope, intUnderlying(args.get(3)));
  word step = step_int.asWord();
  return memoryviewGetslice(thread, self, start, stop, step);
}

RawObject FUNC(_builtins, _mappingproxy_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isMappingProxy()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(mappingproxy));
}

RawObject FUNC(_builtins, _mappingproxy_mapping)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  MappingProxy mappingproxy(&scope, args.get(0));
  return mappingproxy.mapping();
}

RawObject FUNC(_builtins, _mappingproxy_set_mapping)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  MappingProxy mappingproxy(&scope, args.get(0));
  mappingproxy.setMapping(args.get(1));
  return *mappingproxy;
}

RawObject FUNC(_builtins, _memoryview_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isMemoryView());
}

RawObject FUNC(_builtins, _memoryview_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isMemoryView()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(memoryview));
}

RawObject FUNC(_builtins, _memoryview_itemsize)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return thread->raiseRequiresType(self_obj, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);
  return SmallInt::fromWord(memoryviewItemsize(thread, self));
}

RawObject FUNC(_builtins, _memoryview_nbytes)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return thread->raiseRequiresType(self_obj, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);
  return SmallInt::fromWord(self.length());
}

RawObject FUNC(_builtins, _memoryview_setitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return raiseRequiresFromCaller(thread, args, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);
  if (self.readOnly()) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "cannot modify read-only memory");
  }
  Object index_obj(&scope, args.get(1));
  if (!index_obj.isInt()) return Unbound::object();
  Int index_int(&scope, *index_obj);
  word index = index_int.asWord();
  word item_size = memoryviewItemsize(thread, self);
  word byte_index = (index < 0 ? -index : index) * item_size;
  if (byte_index + item_size > self.length()) {
    return thread->raiseWithFmt(LayoutId::kIndexError, "index out of bounds");
  }
  if (index < 0) {
    byte_index = self.length() - byte_index;
  }

  Object value(&scope, args.get(2));
  return memoryviewSetitem(thread, self, byte_index, value);
}

RawObject FUNC(_builtins, _memoryview_setslice)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isMemoryView()) {
    return thread->raiseRequiresType(self_obj, ID(memoryview));
  }
  MemoryView self(&scope, *self_obj);
  if (self.readOnly()) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "cannot modify read-only memory");
  }
  Int start_int(&scope, intUnderlying(args.get(1)));
  word start = start_int.asWord();
  Int stop_int(&scope, intUnderlying(args.get(2)));
  word stop = stop_int.asWord();
  Int step_int(&scope, intUnderlying(args.get(3)));
  word step = step_int.asWord();
  word slice_len = Slice::adjustIndices(self.length(), &start, &stop, step);
  Object value(&scope, args.get(4));
  return memoryviewSetslice(thread, self, start, stop, step, slice_len, value);
}

RawObject FUNC(_builtins, _memoryview_start)(Thread*, Arguments args) {
  return SmallInt::fromWord(MemoryView::cast(args.get(0)).start());
}

RawObject FUNC(_builtins, _mmap_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfMmap(args.get(0)));
}

RawObject FUNC(_builtins, _module_dir)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Module self(&scope, args.get(0));
  return moduleKeys(thread, self);
}

RawObject FUNC(_builtins, _module_proxy)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfModule(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(module));
  }
  Module module(&scope, *self_obj);
  return module.moduleProxy();
}

RawObject FUNC(_builtins, _module_proxy_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isModuleProxy());
}

RawObject FUNC(_builtins, _module_proxy_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isModuleProxy()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(module_proxy));
}

RawObject FUNC(_builtins, _module_proxy_keys)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isModuleProxy()) {
    return thread->raiseRequiresType(self_obj, ID(module_proxy));
  }
  ModuleProxy self(&scope, *self_obj);
  Module module(&scope, self.module());
  DCHECK(module.moduleProxy() == self, "module.proxy != proxy.module");
  return moduleKeys(thread, module);
}

RawObject FUNC(_builtins, _module_proxy_setitem)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isModuleProxy()) {
    return thread->raiseRequiresType(self_obj, ID(module_proxy));
  }
  ModuleProxy self(&scope, *self_obj);
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object value(&scope, args.get(2));
  Module module(&scope, self.module());
  DCHECK(module.moduleProxy() == self, "module.proxy != proxy.module");
  return moduleAtPut(thread, module, name, value);
}

RawObject FUNC(_builtins, _module_proxy_values)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isModuleProxy()) {
    return thread->raiseRequiresType(self_obj, ID(module_proxy));
  }
  ModuleProxy self(&scope, *self_obj);
  Module module(&scope, self.module());
  DCHECK(module.moduleProxy() == self, "module.proxy != proxy.module");
  return moduleValues(thread, module);
}

RawObject FUNC(_builtins, _object_class_set)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self(&scope, args.get(0));

  // Disallow setting __class__ on builtin instances
  Type instance_type(&scope, runtime->typeOf(*self));
  if (instance_type.isBuiltin()) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "__class__ assignment only supported for user types");
  }

  // The new class must be an instance of type
  Object new_type_object(&scope, args.get(1));
  if (!runtime->isInstanceOfType(*new_type_object)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "__class__ must be a type, not a '%T' object",
                                &new_type_object);
  }

  Type new_type(&scope, *new_type_object);
  return typeSetDunderClass(thread, self, new_type);
}

RawObject FUNC(_builtins, _object_keys)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object object(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  Layout layout(&scope, runtime->layoutOf(*object));
  List result(&scope, runtime->newList());
  // Add in-object attributes
  Tuple in_object(&scope, layout.inObjectAttributes());
  word in_object_length = in_object.length();
  word result_length = in_object_length;
  if (layout.hasTupleOverflow()) {
    result_length += Tuple::cast(layout.overflowAttributes()).length();
    // TODO(emacs): Pre-size the tuple in `result' to be result_length.
    static_cast<void>(result_length);
  }
  for (word i = 0; i < in_object_length; i++) {
    Tuple pair(&scope, in_object.at(i));
    Object name(&scope, pair.at(0));
    if (name.isNoneType()) continue;
    AttributeInfo info(pair.at(1));
    if (info.isHidden()) continue;
    runtime->listAdd(thread, result, name);
  }
  // Add overflow attributes
  if (layout.hasTupleOverflow()) {
    Tuple overflow(&scope, layout.overflowAttributes());
    for (word i = 0, length = overflow.length(); i < length; i++) {
      Tuple pair(&scope, overflow.at(i));
      Object name(&scope, pair.at(0));
      if (name == SmallInt::fromWord(0)) continue;
      runtime->listAdd(thread, result, name);
    }
  } else if (layout.hasDictOverflow()) {
    // TODO(T57446141): Dict overflow should be handled by a __dict__ descriptor
    // on the type, like `type` or `function`
    Instance instance(&scope, *object);
    Object overflow_obj(
        &scope, instance.instanceVariableAt(layout.dictOverflowOffset()));
    if (!overflow_obj.isNoneType()) {
      Dict overflow(&scope, *overflow_obj);
      Object key(&scope, NoneType::object());
      Object value(&scope, NoneType::object());
      for (word i = 0; dictNextItem(overflow, &i, &key, &value);) {
        runtime->listAdd(thread, result, key);
      }
    }
  }
  return *result;
}

RawObject FUNC(_builtins, _object_type_getattr)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object instance(&scope, args.get(0));
  Object name(&scope, args.get(1));
  DCHECK(name.isStr(),
         "_object_type_hasattr should only receive string literals");
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInternedStr(thread, name),
         "string literals that look like names should be interned");
  Type type(&scope, runtime->typeOf(*instance));
  Object attr(&scope, typeLookupInMro(thread, *type, *name));
  if (attr.isErrorNotFound()) {
    return Unbound::object();
  }
  if (attr.isFunction()) {
    return runtime->newBoundMethod(attr, instance);
  }
  return resolveDescriptorGet(thread, attr, instance, type);
}

RawObject FUNC(_builtins, _object_type_hasattr)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, thread->runtime()->typeOf(args.get(0)));
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object result(&scope, typeLookupInMro(thread, *type, *name));
  return Bool::fromBool(!result.isErrorNotFound());
}

RawObject FUNC(_builtins, _os_write)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object fd_obj(&scope, args.get(0));
  CHECK(fd_obj.isSmallInt(), "fd must be small int");
  Object byteslike_obj(&scope, args.get(1));
  Byteslike byteslike_buf(&scope, thread, *byteslike_obj);
  DCHECK(byteslike_buf.isValid(), "bytes-like object is invalid");
  size_t count = byteslike_buf.length();
  std::unique_ptr<byte[]> buffer(new byte[count]);
  // TODO(T87798648): We should avoid copies for LargeBytes/DataArray.
  byteslike_buf.copyTo(buffer.get(), count);
  int fd = SmallInt::cast(*fd_obj).value();
  ssize_t result = File::write(fd, buffer.get(), count);
  if (result < 0) {
    return thread->raiseOSErrorFromErrno(-result);
  }
  return SmallInt::fromWord(result);
}

RawObject FUNC(_builtins, _os_error_subclass_from_errno)(Thread* thread,
                                                         Arguments args) {
  HandleScope scope(thread);
  Int errno_value(&scope, intUnderlying(args.get(0)));
  LayoutId subclass = errorLayoutFromErrno(errno_value.asWord());
  return thread->runtime()->typeAt(subclass);
}

RawObject FUNC(_builtins, _profiler_exclude)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object callable(&scope, args.get(0));

  word opcodes_begin = thread->opcodeCount();
  bool enabled = thread->profilingEnabled();
  thread->disableProfiling();
  Object result(&scope, Interpreter::call0(thread, callable));

  word slack = thread->opcodeCount() - opcodes_begin;
  thread->countOpcodes(-slack);
  if (enabled) {
    thread->enableProfiling();
  }
  return *result;
}

static void warnImpreciseCounting(Thread* thread) {
  // The interpreter switching mechanism is currently only applied in a
  // lightweight fashion meaning that recursive interpreter instances further
  // up the stackframe won't actually switch to a counting interpreter.
  bool should_warn = false;
  for (Frame* frame = thread->currentFrame()->previousFrame();
       !frame->isSentinel() && !frame->previousFrame()->isSentinel();
       frame = frame->previousFrame()) {
    if (frame->isNative() ||
        (frame->returnMode() & Frame::kExitRecursiveInterpreter) != 0) {
      should_warn = true;
      break;
    }
  }
  if (should_warn) {
    HandleScope scope(thread);
    Runtime* runtime = thread->runtime();
    Object message(&scope, runtime->newStrFromCStr(
                               "Interpreter switching to count opcodes does "
                               "not affect outer stackframes yet."));
    Object category(&scope, runtime->typeAt(LayoutId::kRuntimeWarning));
    if (ensureBuiltinModuleById(thread, ID(warnings)).isErrorException() ||
        thread->invokeFunction2(ID(warnings), ID(warn), message, category)
            .isErrorException()) {
      thread->ignorePendingException();
    }
  }
}

RawObject FUNC(_builtins, _profiler_install)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object new_thread_func(&scope, args.get(0));
  if (!new_thread_func.isNoneType() &&
      !runtime->isCallable(thread, new_thread_func)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "'_install_profiler' requires a callable or None but got '%T'",
        &new_thread_func);
  }
  Object call_func(&scope, args.get(1));
  Object return_func(&scope, args.get(2));
  if (!call_func.isNoneType() || !return_func.isNoneType()) {
    if (!runtime->isCallable(thread, call_func)) {
      return thread->raiseWithFmt(
          LayoutId::kTypeError,
          "'_install_profiler' requires a callable but got '%T'", &call_func);
    }
    if (!runtime->isCallable(thread, return_func)) {
      return thread->raiseWithFmt(
          LayoutId::kTypeError,
          "'_install_profiler' requires a callable but got '%T'", &return_func);
    }
  }
  if (new_thread_func != runtime->profilingNewThread()) {
    if (!new_thread_func.isNoneType()) {
      Object thread_data(&scope, Interpreter::call0(thread, new_thread_func));
      if (thread_data.isErrorException()) return *thread_data;
      thread->setProfilingData(*thread_data);
    } else {
      thread->setProfilingData(NoneType::object());
    }
  }

  runtime->setProfiling(new_thread_func, call_func, return_func);
  if (call_func.isNoneType()) {
    runtime->interpreter()->setOpcodeCounting(false);
    runtime->reinitInterpreter();
    thread->disableProfiling();
  } else {
    warnImpreciseCounting(thread);
    runtime->interpreter()->setOpcodeCounting(true);
    runtime->reinitInterpreter();
    thread->enableProfiling();
  }
  return NoneType::object();
}

RawObject FUNC(_builtins, _property)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object getter(&scope, args.get(0));
  Object setter(&scope, args.get(1));
  Object deleter(&scope, args.get(2));
  // TODO(T42363565) Do something with the doc argument.
  return thread->runtime()->newProperty(getter, setter, deleter);
}

RawObject FUNC(_builtins, _property_isabstract)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfProperty(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(property));
  }
  Property self(&scope, *self_obj);
  Object getter(&scope, self.getter());
  Object abstract(&scope, isAbstract(thread, getter));
  if (abstract != Bool::falseObj()) {
    return *abstract;
  }
  Object setter(&scope, self.setter());
  if ((abstract = isAbstract(thread, setter)) != Bool::falseObj()) {
    return *abstract;
  }
  Object deleter(&scope, self.deleter());
  return isAbstract(thread, deleter);
}

RawObject FUNC(_builtins, _pyobject_offset)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  NativeProxy proxy(&scope, args.get(0));
  uword addr = reinterpret_cast<uword>(Int::cast(proxy.native()).asCPtr());
  addr += Int::cast(args.get(1)).asWord();
  return thread->runtime()->newIntFromCPtr(reinterpret_cast<void*>(addr));
}

RawObject FUNC(_builtins, _range_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isRange());
}

RawObject FUNC(_builtins, _range_ctor_start_stop)(Thread* thread,
                                                  Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kRange), "unexpected cls");
  HandleScope scope(thread);
  Object start(&scope, args.get(1));
  if (!start.isSmallInt()) {
    start = intFromIndex(thread, start);
    if (start.isError()) {
      return *start;
    }
  }
  Object stop(&scope, args.get(2));
  if (!stop.isSmallInt()) {
    stop = intFromIndex(thread, stop);
    if (stop.isError()) {
      return *stop;
    }
  }
  Object step(&scope, SmallInt::fromWord(1));
  return runtime->newRange(start, stop, step);
}

RawObject FUNC(_builtins, _range_ctor_start_stop_step)(Thread* thread,
                                                       Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kRange), "unexpected cls");
  HandleScope scope(thread);
  Object start(&scope, args.get(1));
  if (!start.isSmallInt()) {
    start = intFromIndex(thread, start);
    if (start.isError()) {
      return *start;
    }
  }
  Object stop(&scope, args.get(2));
  if (!stop.isSmallInt()) {
    stop = intFromIndex(thread, stop);
    if (stop.isError()) {
      return *stop;
    }
  }
  Object step(&scope, args.get(3));
  if (!step.isSmallInt()) {
    step = intFromIndex(thread, step);
    if (step.isError()) {
      return *step;
    }
  }
  Int step_int(&scope, intUnderlying(*step));
  if (step_int.isZero()) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "range() arg 3 must not be zero");
  }
  return runtime->newRange(start, stop, step);
}

RawObject FUNC(_builtins, _range_ctor_stop)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kRange), "unexpected cls");
  HandleScope scope(thread);
  Object start(&scope, SmallInt::fromWord(0));
  Object stop(&scope, args.get(1));
  if (!stop.isSmallInt()) {
    stop = intFromIndex(thread, stop);
    if (stop.isError()) {
      return *stop;
    }
  }
  Object step(&scope, SmallInt::fromWord(1));
  return runtime->newRange(start, stop, step);
}

RawObject FUNC(_builtins, _range_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isRange()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(range));
}

RawObject FUNC(_builtins, _range_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Range self(&scope, args.get(0));
  Object start(&scope, self.start());
  Object stop(&scope, self.stop());
  Object step(&scope, self.step());
  return rangeLen(thread, start, stop, step);
}

RawObject FUNC(_builtins, _readline)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str prompt(&scope, strUnderlying(args.get(0)));
  word length = prompt.length();
  std::unique_ptr<char[]> prompt_buf(new char[length + 1]);
  prompt.copyTo(reinterpret_cast<byte*>(prompt_buf.get()), length);
  prompt_buf.get()[length] = '\0';
  char* line = PyOS_Readline(stdin, stdout, prompt_buf.get());
  if (line == nullptr) {
    CHECK(thread->hasPendingException(), "there must be an exception raised");
    return Error::exception();
  }
  Object result(&scope, thread->runtime()->newStrFromCStr(line));
  std::free(line);
  return *result;
}

RawObject FUNC(_builtins, _repr_enter)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  return thread->reprEnter(obj);
}

RawObject FUNC(_builtins, _repr_leave)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object obj(&scope, args.get(0));
  thread->reprLeave(obj);
  return NoneType::object();
}

RawObject FUNC(_builtins, _seq_index)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  SeqIterator self(&scope, args.get(0));
  return SmallInt::fromWord(self.index());
}

RawObject FUNC(_builtins, _seq_iterable)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  SeqIterator self(&scope, args.get(0));
  return self.iterable();
}

RawObject FUNC(_builtins, _seq_set_index)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  SeqIterator self(&scope, args.get(0));
  Int index(&scope, args.get(1));
  self.setIndex(index.asWord());
  return NoneType::object();
}

RawObject FUNC(_builtins, _seq_set_iterable)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  SeqIterator self(&scope, args.get(0));
  Object iterable(&scope, args.get(1));
  self.setIterable(*iterable);
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfSet(args.get(0)));
}

RawObject FUNC(_builtins,
               _set_function_flag_iterable_coroutine)(Thread* thread,
                                                      Arguments args) {
  HandleScope scope(thread);
  Object function_obj(&scope, args.get(0));
  if (!function_obj.isFunction()) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "can only be called with a 'function' object");
  }
  Function function(&scope, *function_obj);
  Code original_code(&scope, function.code());
  Object code_code(&scope, original_code.code());
  Object consts(&scope, original_code.consts());
  Object names(&scope, original_code.names());
  Object varnames(&scope, original_code.varnames());
  Object freevars(&scope, original_code.freevars());
  Object cellvars(&scope, original_code.cellvars());
  Object filename(&scope, original_code.filename());
  Object name(&scope, original_code.name());
  Object lnotab(&scope, original_code.lnotab());
  Code new_code(
      &scope,
      thread->runtime()->newCode(
          original_code.argcount(), original_code.posonlyargcount(),
          original_code.kwonlyargcount(), original_code.nlocals(),
          original_code.stacksize(),
          original_code.flags() | RawFunction::Flags::kIterableCoroutine,
          code_code, consts, names, varnames, freevars, cellvars, filename,
          name, original_code.firstlineno(), lnotab));
  new_code.setIntrinsic(original_code.intrinsic());
  function.setCode(*new_code);
  function.setFlags(function.flags() | Function::Flags::kIterableCoroutine);
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_ctor)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kSet), "unexpected cls");
  RawObject iterable_raw = args.get(1);
  if (iterable_raw == runtime->emptyTuple()) {
    return runtime->newSet();
  }
  HandleScope scope(thread);
  Object iterable(&scope, iterable_raw);
  Set self(&scope, runtime->newSet());
  return setUpdate(thread, self, iterable);
}

RawObject FUNC(_builtins, _set_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfSet(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(set));
}

RawObject FUNC(_builtins, _set_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Set self(&scope, args.get(0));
  return SmallInt::fromWord(self.numItems());
}

RawObject FUNC(_builtins, _set_member_double)(Thread*, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  double value = Float::cast(args.get(1)).value();
  std::memcpy(reinterpret_cast<void*>(addr), &value, sizeof(value));
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_member_float)(Thread*, Arguments args) {
  auto addr = Int::cast(args.get(0)).asCPtr();
  float value = Float::cast(args.get(1)).value();
  std::memcpy(reinterpret_cast<void*>(addr), &value, sizeof(value));
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_member_integral)(Thread*, Arguments args) {
  void* addr = Int::cast(args.get(0)).asCPtr();
  OptInt<long long> optint = RawInt::cast(args.get(1)).asInt<long long>();
  CHECK(optint.error == CastError::None, "Overflow casting to primitive type");
  long long value = optint.value;
  word num_bytes = RawInt::cast(args.get(2)).asWord();
  static_assert(endian::native == endian::little, "expected little endian");
  std::memcpy(reinterpret_cast<void*>(addr), &value, num_bytes);
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_member_integral_unsigned)(Thread*,
                                                         Arguments args) {
  void* addr = Int::cast(args.get(0)).asCPtr();
  OptInt<unsigned long long> optint =
      RawInt::cast(args.get(1)).asInt<unsigned long long>();
  CHECK(optint.error == CastError::None, "Overflow casting to primitive type");
  unsigned long long value = optint.value;
  word num_bytes = RawInt::cast(args.get(2)).asWord();
  static_assert(endian::native == endian::little, "expected little endian");
  std::memcpy(reinterpret_cast<void*>(addr), &value, num_bytes);
  return NoneType::object();
}

RawObject FUNC(_builtins, _set_member_pyobject)(Thread* thread,
                                                Arguments args) {
  objectSetMember(thread->runtime(), args.get(0), args.get(1));
  return NoneType::object();
}

RawObject FUNC(_builtins, _slice_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isSlice());
}

RawObject FUNC(_builtins, _slice_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isSlice()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(slice));
}

RawObject FUNC(_builtins, _slice_start)(Thread*, Arguments args) {
  RawObject start_obj = args.get(0);
  word step = SmallInt::cast(args.get(1)).value();
  word length = SmallInt::cast(args.get(2)).value();
  if (start_obj.isNoneType()) {
    return SmallInt::fromWord(step < 0 ? length - 1 : 0);
  }

  word lower, upper;
  if (step < 0) {
    lower = -1;
    upper = length - 1;
  } else {
    lower = 0;
    upper = length;
  }

  word start = intUnderlying(start_obj).asWordSaturated();
  if (start < 0) {
    start = Utils::maximum(start + length, lower);
  } else {
    start = Utils::minimum(start, upper);
  }
  return SmallInt::fromWord(start);
}

RawObject FUNC(_builtins, _staticmethod)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object function(&scope, args.get(0));

  StaticMethod method(&scope, thread->runtime()->newStaticMethod());
  method.setFunction(*function);
  return *method;
}

RawObject FUNC(_builtins, _slice_start_long)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Int step(&scope, intUnderlying(args.get(1)));
  Int length(&scope, intUnderlying(args.get(2)));
  bool negative_step = step.isNegative();
  Int lower(&scope, SmallInt::fromWord(negative_step ? -1 : 0));
  Runtime* runtime = thread->runtime();
  // upper = length + lower; if step < 0, then lower = 0 anyway
  Int upper(&scope,
            negative_step ? runtime->intAdd(thread, length, lower) : *length);
  Object start_obj(&scope, args.get(0));
  if (start_obj.isNoneType()) {
    return negative_step ? *upper : *lower;
  }
  Int start(&scope, intUnderlying(*start_obj));
  if (start.isNegative()) {
    start = runtime->intAdd(thread, start, length);
    if (start.compare(*lower) < 0) {
      start = *lower;
    }
  } else if (start.compare(*upper) > 0) {
    start = *upper;
  }
  return *start;
}

bool FUNC(_builtins, _slice_step_intrinsic)(Thread* thread) {
  RawObject step_obj = thread->stackTop();
  if (step_obj.isNoneType()) {
    thread->stackPop();
    thread->stackSetTop(SmallInt::fromWord(1));
    return true;
  }
  if (step_obj.isSmallInt()) {
    thread->stackPop();
    thread->stackSetTop(step_obj);
    return true;
  }
  return false;
}

RawObject FUNC(_builtins, _slice_step)(Thread* thread, Arguments args) {
  RawObject step_obj = args.get(0);
  if (step_obj.isNoneType()) return SmallInt::fromWord(1);
  RawInt step = intUnderlying(step_obj);
  if (step == SmallInt::fromWord(0) || step == Bool::falseObj()) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "slice step cannot be zero");
  }
  if (step.isSmallInt()) {
    return step;
  }
  if (step == Bool::trueObj()) {
    return SmallInt::fromWord(1);
  }
  return SmallInt::fromWord(step.isNegative() ? SmallInt::kMinValue
                                              : SmallInt::kMaxValue);
}

RawObject FUNC(_builtins, _slice_step_long)(Thread* thread, Arguments args) {
  RawObject step_obj = args.get(0);
  if (step_obj.isNoneType()) return SmallInt::fromWord(1);
  RawInt step = intUnderlying(step_obj);
  if (step == SmallInt::fromWord(0) || step == Bool::falseObj()) {
    return thread->raiseWithFmt(LayoutId::kValueError,
                                "slice step cannot be zero");
  }
  if (step.isSmallInt()) {
    return step;
  }
  if (step == Bool::trueObj()) {
    return SmallInt::fromWord(1);
  }
  return step;
}

RawObject FUNC(_builtins, _slice_stop)(Thread*, Arguments args) {
  RawObject stop_obj = args.get(0);
  word step = SmallInt::cast(args.get(1)).value();
  word length = SmallInt::cast(args.get(2)).value();
  if (stop_obj.isNoneType()) {
    return SmallInt::fromWord(step < 0 ? -1 : length);
  }

  word lower, upper;
  if (step < 0) {
    lower = -1;
    upper = length - 1;
  } else {
    lower = 0;
    upper = length;
  }

  word stop = intUnderlying(stop_obj).asWordSaturated();
  if (stop < 0) {
    stop = Utils::maximum(stop + length, lower);
  } else {
    stop = Utils::minimum(stop, upper);
  }
  return SmallInt::fromWord(stop);
}

RawObject FUNC(_builtins, _slice_stop_long)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Int step(&scope, intUnderlying(args.get(1)));
  Int length(&scope, intUnderlying(args.get(2)));
  bool negative_step = step.isNegative();
  Int lower(&scope, SmallInt::fromWord(negative_step ? -1 : 0));
  Runtime* runtime = thread->runtime();
  // upper = length + lower; if step < 0, then lower = 0 anyway
  Int upper(&scope,
            negative_step ? runtime->intAdd(thread, length, lower) : *length);
  Object stop_obj(&scope, args.get(0));
  if (stop_obj.isNoneType()) {
    return negative_step ? *lower : *upper;
  }
  Int stop(&scope, intUnderlying(*stop_obj));
  if (stop.isNegative()) {
    stop = runtime->intAdd(thread, stop, length);
    if (stop.compare(*lower) < 0) {
      stop = *lower;
    }
  } else if (stop.compare(*upper) > 0) {
    stop = *upper;
  }
  return *stop;
}

RawObject FUNC(_builtins, _staticmethod_isabstract)(Thread* thread,
                                                    Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfStaticMethod(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(staticmethod));
  }
  StaticMethod self(&scope, *self_obj);
  Object func(&scope, self.function());
  return isAbstract(thread, func);
}

RawObject FUNC(_builtins, _stop_iteration_ctor)(Thread* thread,
                                                Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kStopIteration),
         "unexpected type; should be StopIteration");
  Layout layout(&scope, runtime->layoutAt(LayoutId::kStopIteration));
  StopIteration self(&scope, runtime->newInstance(layout));
  Object args_obj(&scope, args.get(1));
  self.setArgs(*args_obj);
  self.setCause(Unbound::object());
  self.setContext(Unbound::object());
  self.setTraceback(Unbound::object());
  self.setSuppressContext(RawBool::falseObj());
  Tuple tuple(&scope, self.args());
  if (tuple.length() > 0) self.setValue(tuple.at(0));
  return *self;
}

RawObject FUNC(_builtins, _str_array_clear)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  StrArray self(&scope, args.get(0));
  self.setNumItems(0);
  return NoneType::object();
}

RawObject FUNC(_builtins, _str_array_ctor)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kStrArray),
         "_str_array.__new__(X): X is not '_str_array'");
  Object self_obj(&scope, runtime->newStrArray());
  if (self_obj.isError()) return *self_obj;
  StrArray self(&scope, *self_obj);
  self.setNumItems(0);
  Object source_obj(&scope, args.get(1));
  if (source_obj.isUnbound()) {
    return *self;
  }
  if (!runtime->isInstanceOfStr(*source_obj)) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "_str_array can only be initialized with str");
  }
  Str source(&scope, strUnderlying(*source_obj));
  runtime->strArrayAddStr(thread, self, source);
  return *self;
}

RawObject FUNC(_builtins, _str_array_iadd)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  StrArray self(&scope, args.get(0));
  Str other(&scope, strUnderlying(args.get(1)));
  thread->runtime()->strArrayAddStr(thread, self, other);
  return *self;
}

RawObject FUNC(_builtins, _structseq_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  word index = SmallInt::cast(args.get(1)).value();
  return structseqGetItem(thread, self, index);
}

RawObject FUNC(_builtins, _structseq_new_type)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Str name(&scope, strUnderlying(args.get(0)));
  name = Runtime::internStr(thread, name);
  Tuple field_names(&scope, args.get(1));
  Bool is_heaptype(&scope, args.get(2));
  word num_fields = field_names.length();
  if (num_fields > 0) {
    MutableTuple field_names_interned(&scope,
                                      runtime->newMutableTuple(num_fields));
    Object field_name(&scope, NoneType::object());
    for (word i = 0; i < num_fields; i++) {
      field_name = field_names.at(i);
      if (!field_name.isNoneType()) {
        field_name = Runtime::internStr(thread, field_name);
      }
      field_names_interned.atPut(i, *field_name);
    }
    field_names = field_names_interned.becomeImmutable();
  }
  word num_in_sequence = args.get(3).isUnbound()
                             ? num_fields
                             : SmallInt::cast(args.get(3)).value();
  word flags =
      is_heaptype.value() ? Type::Flag::kIsCPythonHeaptype : Type::Flag::kNone;
  flags |= Type::Flag::kIsStructseq;
  return structseqNewType(thread, name, field_names, num_in_sequence, flags);
}

RawObject FUNC(_builtins, _structseq_setitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self(&scope, args.get(0));
  word index = SmallInt::cast(args.get(1)).value();
  Object value(&scope, args.get(2));
  return structseqSetItem(thread, self, index, value);
}

static RawObject padString(Thread* thread, const Str& str,
                           const SmallStr& fillchar, word str_length,
                           word left_padding, word fill_char_length,
                           word result_length) {
  // Optimize to use SmallStr for results less than the small string max length
  if (result_length <= SmallStr::kMaxLength) {
    byte buffer[SmallStr::kMaxLength];
    for (word i = 0; i < left_padding; i += fill_char_length) {
      fillchar.copyTo(&buffer[i], fill_char_length);
    }
    str.copyTo(&buffer[left_padding], str_length);
    for (word i = left_padding + str_length; i < result_length;
         i += fill_char_length) {
      fillchar.copyTo(&buffer[i], fill_char_length);
    }

    return SmallStr::fromBytes({buffer, result_length});
  }

  HandleScope scope(thread);

  MutableBytes buffer(
      &scope, thread->runtime()->newMutableBytesUninitialized(result_length));

  {
    // In order to improve performance for string operations we write directly
    // to the memory address of the buffer. This operation requires NO calls
    // which could potentially trigger allocations in order to ensure memory
    // consistency.
    byte* dst = reinterpret_cast<byte*>(buffer.address());

    for (word i = 0; i < left_padding; i += fill_char_length) {
      fillchar.copyTo(&dst[i], fill_char_length);
    }
    str.copyTo(&dst[left_padding], str_length);
    for (word i = left_padding + str_length; i < result_length;
         i += fill_char_length) {
      fillchar.copyTo(&dst[i], fill_char_length);
    }
  }

  return buffer.becomeStr();
}
RawObject FUNC(_builtins, _str_center)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }

  Object width_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*width_obj)) {
    return Unbound::object();
  }
  Int width_int(&scope, intUnderlying(*width_obj));
  if (width_int.isLargeInt()) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "int too large to convert to an index");
  }
  word width = width_int.asWord();

  Object fillchar_obj(&scope, args.get(2));
  if (!runtime->isInstanceOfStr(*fillchar_obj)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be a unicode character, not a '%T'",
        &fillchar_obj);
  }

  Str fillchar_str(&scope, strUnderlying(*fillchar_obj));
  if (!fillchar_str.isSmallStr() || fillchar_str.codePointLength() != 1) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be exactly one character long");
  }

  SmallStr fillchar(&scope, *fillchar_str);

  Str self(&scope, strUnderlying(*self_obj));

  word self_codepoints = self.codePointLength();
  if (self_codepoints >= width) {
    return *self;
  }

  word self_length = self.length();
  word fill_char_length = fillchar.length();
  word fill_char_count = width - self_codepoints;
  word fill_length = fill_char_length * fill_char_count;
  word result_length = self_length + fill_length;

  word left_padding = fill_char_count / 2;

  // When fill characters cannot be evenly distributed place the extra on the
  // left.
  if (fill_char_count % 2 != 0 && width % 2 != 0) {
    left_padding += 1;
  }
  left_padding *= fill_char_length;

  return padString(thread, self, fillchar, self_length, left_padding,
                   fill_char_length, result_length);
}

RawObject FUNC(_builtins, _str_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfStr(args.get(0)));
}

RawObject FUNC(_builtins, _str_encode)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object str_obj(&scope, args.get(0));
  if (!str_obj.isStr()) {
    return Unbound::object();
  }
  Str str(&scope, *str_obj);
  static RawSmallStr ascii = SmallStr::fromCStr("ascii");
  static RawSmallStr utf8 = SmallStr::fromCStr("utf-8");
  static RawSmallStr latin1 = SmallStr::fromCStr("latin-1");
  Str enc(&scope, args.get(1));
  if (enc != ascii && enc != utf8 && enc != latin1 &&
      enc.compareCStr("iso-8859-1") != 0) {
    return Unbound::object();
  }
  return strEncodeASCII(thread, str);
}

RawObject FUNC(_builtins, _str_encode_ascii)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object str_obj(&scope, args.get(0));
  if (!str_obj.isStr()) {
    return Unbound::object();
  }
  Str str(&scope, *str_obj);
  return strEncodeASCII(thread, str);
}

RawObject FUNC(_builtins, _str_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isStr());
}

RawObject FUNC(_builtins, _str_compare_digest)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  // TODO(T57794178): Use volatile
  Object left_obj(&scope, args.get(0));
  Object right_obj(&scope, args.get(1));
  DCHECK(runtime->isInstanceOfStr(*left_obj),
         "_str_compare_digest requires 'str' instance");
  DCHECK(runtime->isInstanceOfStr(*right_obj),
         "_str_compare_digest requires 'str' instance");
  Str left(&scope, strUnderlying(*left_obj));
  Str right(&scope, strUnderlying(*right_obj));
  if (!left.isASCII() || !right.isASCII()) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "comparing strings with non-ASCII characters is not supported");
  }
  word left_len = left.length();
  word right_len = right.length();
  word length = Utils::minimum(left_len, right_len);
  word result = (right_len == left_len) ? 0 : 1;
  for (word i = 0; i < length; i++) {
    result |= left.byteAt(i) ^ right.byteAt(i);
  }
  return Bool::fromBool(result == 0);
}

RawObject FUNC(_builtins, _str_count)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfStr(args.get(0)),
         "_str_count requires 'str' instance");
  DCHECK(runtime->isInstanceOfStr(args.get(1)),
         "_str_count requires 'str' instance");
  HandleScope scope(thread);
  Str haystack(&scope, strUnderlying(args.get(0)));
  Str needle(&scope, strUnderlying(args.get(1)));
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  word start = 0;
  if (!start_obj.isNoneType()) {
    start = intUnderlying(*start_obj).asWordSaturated();
  }
  word end = kMaxWord;
  if (!end_obj.isNoneType()) {
    end = intUnderlying(*end_obj).asWordSaturated();
  }
  return strCount(haystack, needle, start, end);
}

RawObject FUNC(_builtins, _str_ctor)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object cls(&scope, args.get(0));
  Object obj(&scope, args.get(1));
  Object encoding(&scope, args.get(2));
  Object errors(&scope, args.get(3));
  Runtime* runtime = thread->runtime();
  DCHECK(*cls == runtime->typeAt(LayoutId::kStr), "unexpected cls");
  if (obj.isUnbound()) {
    return Str::empty();
  }
  if (encoding.isUnbound() && errors.isUnbound() && obj.isStr()) {
    return *obj;
  }
  Type str_type(&scope, runtime->typeAt(LayoutId::kStr));
  Object dunder_new(&scope, runtime->symbols()->at(ID(__new__)));
  Function str_dunder_new(&scope,
                          typeGetAttribute(thread, str_type, dunder_new));
  // TODO(T76654356): Use Thread::invokeMethodStatic.
  return Interpreter::call4(thread, str_dunder_new, cls, obj, encoding, errors);
}

RawObject FUNC(_builtins, _str_ctor_obj)(Thread* thread, Arguments args) {
  {
    // Warning: This code is using `RawXXX` variables for performance! This is
    // despite the fact that we call functions that do potentially perform
    // memory allocations. Be careful not to break this invariant if you change
    // the code!
    DCHECK(args.get(0) == thread->runtime()->typeAt(LayoutId::kStr),
           "expected cls==str");
    RawObject obj_raw = args.get(1);
    if (obj_raw.isStr()) {
      return obj_raw;
    }
  }
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Type str_type(&scope, runtime->typeAt(LayoutId::kStr));
  Object dunder_new(&scope, runtime->symbols()->at(ID(__new__)));
  Function str_dunder_new(&scope,
                          typeGetAttribute(thread, str_type, dunder_new));
  Object cls(&scope, args.get(0));
  Object obj(&scope, args.get(1));
  Object encoding(&scope, Unbound::object());
  Object errors(&scope, Unbound::object());
  // TODO(T76654356): Use Thread::invokeMethodStatic.
  return Interpreter::call4(thread, str_dunder_new, cls, obj, encoding, errors);
}

RawObject FUNC(_builtins, _str_endswith)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  Str self(&scope, strUnderlying(args.get(0)));
  Str suffix(&scope, strUnderlying(args.get(1)));

  word len = self.codePointLength();
  word start = 0;
  word end = len;
  if (!start_obj.isNoneType()) {
    // TODO(T55084422): bounds checking
    start = intUnderlying(*start_obj).asWordSaturated();
  }
  if (!end_obj.isNoneType()) {
    // TODO(T55084422): bounds checking
    end = intUnderlying(*end_obj).asWordSaturated();
  }

  Slice::adjustSearchIndices(&start, &end, len);
  word suffix_len = suffix.codePointLength();
  if (start + suffix_len > end) {
    return Bool::falseObj();
  }
  word start_offset = thread->strOffset(self, end - suffix_len);
  word suffix_chars = suffix.length();
  for (word i = start_offset, j = 0; j < suffix_chars; i++, j++) {
    if (self.byteAt(i) != suffix.byteAt(j)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject FUNC(_builtins, _str_escape_non_ascii)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  CHECK(thread->runtime()->isInstanceOfStr(args.get(0)),
        "_str_escape_non_ascii expected str instance");
  Str obj(&scope, strUnderlying(args.get(0)));
  return strEscapeNonASCII(thread, obj);
}

RawObject FUNC(_builtins, _str_find)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfStr(args.get(0)),
         "_str_find requires 'str' instance");
  DCHECK(runtime->isInstanceOfStr(args.get(1)),
         "_str_find requires 'str' instance");
  HandleScope scope(thread);
  Str haystack(&scope, strUnderlying(args.get(0)));
  Str needle(&scope, strUnderlying(args.get(1)));
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  if ((start_obj.isNoneType() || start_obj == SmallInt::fromWord(0)) &&
      end_obj.isNoneType()) {
    return SmallInt::fromWord(strFind(haystack, needle));
  }
  word start = 0;
  if (!start_obj.isNoneType()) {
    start = intUnderlying(*start_obj).asWordSaturated();
  }
  word end = kMaxWord;
  if (!end_obj.isNoneType()) {
    end = intUnderlying(*end_obj).asWordSaturated();
  }
  word result = strFindWithRange(haystack, needle, start, end);
  return SmallInt::fromWord(result);
}

RawObject FUNC(_builtins, _str_from_str)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  DCHECK(type.builtinBase() == LayoutId::kStr, "type must subclass str");
  Str value(&scope, strUnderlying(args.get(1)));
  if (type.isBuiltin()) return *value;
  Layout type_layout(&scope, type.instanceLayout());
  UserStrBase instance(&scope, thread->runtime()->newInstance(type_layout));
  instance.setValue(*value);
  return *instance;
}

RawObject FUNC(_builtins, _str_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(str));
  }
  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    Str self(&scope, strUnderlying(*self_obj));
    word index = intUnderlying(*key).asWordSaturated();
    if (!SmallInt::isValid(index)) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }
    if (index < 0) {
      index += self.codePointLength();
    }
    if (index >= 0) {
      word offset = thread->strOffset(self, index);
      if (offset < self.length()) {
        word ignored;
        return SmallStr::fromCodePoint(self.codePointAt(offset, &ignored));
      }
    }
    return thread->raiseWithFmt(LayoutId::kIndexError,
                                "string index out of range");
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  // Manually adjust slice bounds to avoid an extra call to codePointLength
  Str self(&scope, strUnderlying(*self_obj));
  word start_index = adjustedStrIndex(self, start);
  word stop_index = adjustedStrIndex(self, stop);
  word length = stop_index - start_index;
  if (length <= 0) return Str::empty();
  return strSubstr(thread, self, start_index, length);
}

RawObject FUNC(_builtins, _str_getslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str self(&scope, strUnderlying(args.get(0)));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  return thread->runtime()->strSlice(thread, self, start, stop, step);
}

RawObject FUNC(_builtins, _str_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfStr(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(str));
}

RawObject FUNC(_builtins, _str_ischr)(Thread*, Arguments args) {
  RawStr str = strUnderlying(args.get(0));
  return Bool::fromBool(str.isSmallStr() && str.codePointLength() == 1);
}

RawObject FUNC(_builtins, _str_join)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object sep_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*sep_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(str));
  }
  Str sep(&scope, strUnderlying(*sep_obj));
  Object iterable(&scope, args.get(1));
  return strJoinWithTupleOrList(thread, sep, iterable);
}

RawObject FUNC(_builtins, _str_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str self(&scope, strUnderlying(args.get(0)));
  return SmallInt::fromWord(self.codePointLength());
}

RawObject FUNC(_builtins, _str_ljust)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }

  Object width_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*width_obj)) {
    return Unbound::object();
  }

  Int width_int(&scope, intUnderlying(*width_obj));
  if (width_int.isLargeInt()) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "int too large to convert to an index");
  }
  word width = width_int.asWord();

  Object fillchar_obj(&scope, args.get(2));
  if (!runtime->isInstanceOfStr(*fillchar_obj)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be a unicode character, not a '%T'",
        &fillchar_obj);
  }

  Str fillchar_str(&scope, strUnderlying(*fillchar_obj));
  if (!fillchar_str.isSmallStr() || fillchar_str.codePointLength() != 1) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be exactly one character long");
  }

  SmallStr fillchar(&scope, *fillchar_str);

  Str self(&scope, strUnderlying(*self_obj));

  word self_codepoints = self.codePointLength();
  if (self_codepoints >= width) {
    return *self;
  }

  word self_length = self.length();
  word fill_char_length = fillchar.length();
  word fill_char_count = width - self_codepoints;
  word fill_length = fill_char_length * fill_char_count;
  word result_length = self_length + fill_length;

  word left_padding = 0;
  return padString(thread, self, fillchar, self_length, left_padding,
                   fill_char_length, result_length);
}
RawObject FUNC(_builtins, _str_mod_fast_path)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(args.get(0)) ||
      !runtime->isInstanceOfTuple(args.get(1))) {
    return Unbound::object();
  }
  HandleScope scope(thread);
  Str str(&scope, strUnderlying(args.get(0)));
  Tuple args_tuple(&scope, tupleUnderlying(args.get(1)));
  const word max_args = 16;
  word num_args = args_tuple.length();
  if (num_args > max_args) {
    return Unbound::object();
  }

  // Scan format string for occurences of %s and remember their indexes. Also
  // check that the corresponding arguments are strings.
  word arg_indexes[max_args];
  word arg_idx = 0;
  word result_length = 0;
  Object arg(&scope, Unbound::object());
  word fmt_length = str.length();
  for (word i = 0; i < fmt_length; i++) {
    if (str.byteAt(i) != '%') {
      result_length++;
      continue;
    }
    i++;
    if (i >= fmt_length || str.byteAt(i) != 's' || arg_idx >= num_args) {
      return Unbound::object();
    }
    arg = args_tuple.at(arg_idx);
    if (!arg.isStr()) {
      return Unbound::object();
    }
    result_length += Str::cast(*arg).length();
    arg_indexes[arg_idx] = i - 1;
    arg_idx++;
  }
  if (arg_idx < num_args) {
    return Unbound::object();
  }

  // Construct resulting string.
  if (arg_idx == 0) {
    return *str;
  }
  MutableBytes result(&scope,
                      runtime->newMutableBytesUninitialized(result_length));
  word result_idx = 0;
  word fmt_idx = 0;
  Str arg_str(&scope, Str::empty());
  for (word a = 0; a < num_args; a++) {
    word fragment_begin = fmt_idx;
    word fragment_length = arg_indexes[a] - fragment_begin;
    result.replaceFromWithStrStartAt(result_idx, *str, fragment_length,
                                     fragment_begin);
    result_idx += fragment_length;
    fmt_idx += fragment_length + 2;

    arg_str = args_tuple.at(a);
    word arg_length = arg_str.length();
    result.replaceFromWithStr(result_idx, *arg_str, arg_length);
    result_idx += arg_length;
  }
  word fragment_begin = fmt_idx;
  word fragment_length = fmt_length - fmt_idx;
  result.replaceFromWithStrStartAt(result_idx, *str, fragment_length,
                                   fragment_begin);
  return result.becomeStr();
}

static word strScan(const Str& haystack, word haystack_len, const Str& needle,
                    word needle_len,
                    word (*find_func)(const byte* haystack, word haystack_len,
                                      const byte* needle, word needle_len)) {
  byte haystack_buf[SmallStr::kMaxLength];
  byte* haystack_ptr = haystack_buf;
  if (haystack.isSmallStr()) {
    haystack.copyTo(haystack_buf, haystack_len);
  } else {
    haystack_ptr = reinterpret_cast<byte*>(LargeStr::cast(*haystack).address());
  }
  byte needle_buf[SmallStr::kMaxLength];
  byte* needle_ptr = needle_buf;
  if (needle.isSmallStr()) {
    needle.copyTo(needle_buf, needle_len);
  } else {
    needle_ptr = reinterpret_cast<byte*>(LargeStr::cast(*needle).address());
  }
  return (*find_func)(haystack_ptr, haystack_len, needle_ptr, needle_len);
}

// Look for needle in haystack, starting from the left. Return a tuple
// containing:
// * haystack up to but not including needle
// * needle
// * haystack after and not including needle
// If needle is not found in haystack, return (haystack, "", "")
RawObject FUNC(_builtins, _str_partition)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str haystack(&scope, strUnderlying(args.get(0)));
  Str needle(&scope, strUnderlying(args.get(1)));
  Runtime* runtime = thread->runtime();
  MutableTuple result(&scope, runtime->newMutableTuple(3));
  result.atPut(0, *haystack);
  result.atPut(1, Str::empty());
  result.atPut(2, Str::empty());
  word haystack_len = haystack.length();
  word needle_len = needle.length();
  if (haystack_len < needle_len) {
    // Fast path when needle is bigger than haystack
    return result.becomeImmutable();
  }
  word prefix_len =
      strScan(haystack, haystack_len, needle, needle_len, Utils::memoryFind);
  if (prefix_len < 0) return result.becomeImmutable();
  result.atPut(0, strSubstr(thread, haystack, 0, prefix_len));
  result.atPut(1, *needle);
  word suffix_start = prefix_len + needle_len;
  word suffix_len = haystack_len - suffix_start;
  result.atPut(2, strSubstr(thread, haystack, suffix_start, suffix_len));
  return result.becomeImmutable();
}

RawObject FUNC(_builtins, _str_replace)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Str self(&scope, strUnderlying(args.get(0)));
  Str oldstr(&scope, strUnderlying(args.get(1)));
  Str newstr(&scope, strUnderlying(args.get(2)));
  word count = intUnderlying(args.get(3)).asWordSaturated();
  return runtime->strReplace(thread, self, oldstr, newstr, count);
}

RawObject FUNC(_builtins, _str_rfind)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfStr(args.get(0)),
         "_str_rfind requires 'str' instance");
  DCHECK(runtime->isInstanceOfStr(args.get(1)),
         "_str_rfind requires 'str' instance");
  HandleScope scope(thread);
  Str haystack(&scope, strUnderlying(args.get(0)));
  Str needle(&scope, strUnderlying(args.get(1)));
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  word start = 0;
  if (!start_obj.isNoneType()) {
    start = intUnderlying(*start_obj).asWordSaturated();
  }
  word end = kMaxWord;
  if (!end_obj.isNoneType()) {
    end = intUnderlying(*end_obj).asWordSaturated();
  }
  Slice::adjustSearchIndices(&start, &end, haystack.codePointLength());
  word result = strRFind(haystack, needle, start, end);
  return SmallInt::fromWord(result);
}

RawObject FUNC(_builtins, _str_rjust)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(str));
  }

  Object width_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfInt(*width_obj)) {
    return Unbound::object();
  }
  Int width_int(&scope, intUnderlying(*width_obj));
  if (width_int.isLargeInt()) {
    return thread->raiseWithFmt(LayoutId::kOverflowError,
                                "int too large to convert to an index");
  }
  word width = width_int.asWord();

  Object fillchar_obj(&scope, args.get(2));
  if (!runtime->isInstanceOfStr(*fillchar_obj)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be a unicode character, not a '%T'",
        &fillchar_obj);
  }

  Str fillchar_str(&scope, strUnderlying(*fillchar_obj));
  if (!fillchar_str.isSmallStr() || fillchar_str.codePointLength() != 1) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "The fill character must be exactly one character long");
  }

  SmallStr fillchar(&scope, *fillchar_str);

  Str self(&scope, strUnderlying(*self_obj));

  word self_codepoints = self.codePointLength();
  if (self_codepoints >= width) {
    return *self;
  }

  word self_length = self.length();
  word fill_char_length = fillchar.length();
  word fill_char_count = width - self_codepoints;
  word fill_length = fill_char_length * fill_char_count;
  word result_length = self_length + fill_length;

  word left_padding = fill_length;
  return padString(thread, self, fillchar, self_length, left_padding,
                   fill_char_length, result_length);
}

// Look for needle in haystack, starting from the right. Return a tuple
// containing:
// * haystack up to but not including needle
// * needle
// * haystack after and not including needle
// If needle is not found in haystack, return ("", "", haystack)
RawObject FUNC(_builtins, _str_rpartition)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Str haystack(&scope, strUnderlying(args.get(0)));
  Str needle(&scope, strUnderlying(args.get(1)));
  MutableTuple result(&scope, runtime->newMutableTuple(3));
  result.atPut(0, Str::empty());
  result.atPut(1, Str::empty());
  result.atPut(2, *haystack);
  word haystack_len = haystack.length();
  word needle_len = needle.length();
  if (haystack_len < needle_len) {
    // Fast path when needle is bigger than haystack
    return result.becomeImmutable();
  }
  word prefix_len = strScan(haystack, haystack_len, needle, needle_len,
                            Utils::memoryFindReverse);
  if (prefix_len < 0) return result.becomeImmutable();
  result.atPut(0, strSubstr(thread, haystack, 0, prefix_len));
  result.atPut(1, *needle);
  word suffix_start = prefix_len + needle_len;
  word suffix_len = haystack_len - suffix_start;
  result.atPut(2, strSubstr(thread, haystack, suffix_start, suffix_len));
  return result.becomeImmutable();
}

static RawObject strSplitWhitespace(Thread* thread, const Str& self,
                                    word maxsplit) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  List result(&scope, runtime->newList());
  if (maxsplit < 0) {
    maxsplit = kMaxWord;
  }
  word self_length = self.length();
  word num_split = 0;
  Str substr(&scope, Str::empty());
  for (word i = 0, j = 0; j < self_length; i = self.offsetByCodePoints(j, 1)) {
    // Find beginning of next word
    {
      word num_bytes;
      while (i < self_length &&
             Unicode::isSpace(self.codePointAt(i, &num_bytes))) {
        i += num_bytes;
      }
    }
    if (i == self_length) {
      // End of string; finished
      break;
    }

    // Find end of next word
    if (maxsplit == num_split) {
      // Take the rest of the string
      j = self_length;
    } else {
      j = self.offsetByCodePoints(i, 1);
      {
        word num_bytes;
        while (j < self_length &&
               !Unicode::isSpace(self.codePointAt(j, &num_bytes))) {
          j += num_bytes;
        }
      }
      num_split += 1;
    }
    substr = strSubstr(thread, self, i, j - i);
    runtime->listAdd(thread, result, substr);
  }
  return *result;
}

RawObject FUNC(_builtins, _str_split)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Str self(&scope, strUnderlying(args.get(0)));
  Object sep_obj(&scope, args.get(1));
  word maxsplit = intUnderlying(args.get(2)).asWordSaturated();
  if (sep_obj.isNoneType()) {
    return strSplitWhitespace(thread, self, maxsplit);
  }
  Str sep(&scope, strUnderlying(*sep_obj));
  if (sep.length() == 0) {
    return thread->raiseWithFmt(LayoutId::kValueError, "empty separator");
  }
  if (maxsplit < 0) {
    maxsplit = kMaxWord;
  }
  return strSplit(thread, self, sep, maxsplit);
}

RawObject FUNC(_builtins, _str_splitlines)(Thread* thread, Arguments args) {
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfStr(args.get(0)),
         "_str_splitlines requires 'str' instance");
  DCHECK(runtime->isInstanceOfInt(args.get(1)),
         "_str_splitlines requires 'int' instance");
  HandleScope scope(thread);
  Str self(&scope, args.get(0));
  bool keepends = !intUnderlying(args.get(1)).isZero();
  return strSplitlines(thread, self, keepends);
}

RawObject FUNC(_builtins, _str_startswith)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object start_obj(&scope, args.get(2));
  Object end_obj(&scope, args.get(3));
  Str self(&scope, strUnderlying(args.get(0)));
  Str prefix(&scope, strUnderlying(args.get(1)));

  word len = self.codePointLength();
  word start = 0;
  word end = len;
  if (!start_obj.isNoneType()) {
    // TODO(T55084422): bounds checking
    start = intUnderlying(*start_obj).asWordSaturated();
  }
  if (!end_obj.isNoneType()) {
    // TODO(T55084422): bounds checking
    end = intUnderlying(*end_obj).asWordSaturated();
  }

  Slice::adjustSearchIndices(&start, &end, len);
  if (start + prefix.codePointLength() > end) {
    return Bool::falseObj();
  }
  word start_offset = thread->strOffset(self, start);
  word prefix_chars = prefix.length();
  for (word i = start_offset, j = 0; j < prefix_chars; i++, j++) {
    if (self.byteAt(i) != prefix.byteAt(j)) {
      return Bool::falseObj();
    }
  }
  return Bool::trueObj();
}

RawObject FUNC(_builtins, _str_translate)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, strUnderlying(args.get(0)));
  if (!runtime->isInstanceOfStr(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(str));
  }
  Str self(&scope, *self_obj);
  Object table_obj(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*table_obj)) {
    return Unbound::object();
  }
  Str table(&scope, strUnderlying(*table_obj));
  return strTranslateASCII(thread, self, table);
}

RawObject FUNC(_builtins, _super)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object cls(&scope, args.get(0));
  Super result(&scope, thread->runtime()->newSuper());
  result.setType(*cls);
  result.setObject(*cls);
  result.setObjectType(*cls);
  return *result;
}

RawObject FUNC(_builtins, _super_ctor)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  DCHECK(args.get(0) == runtime->typeAt(LayoutId::kSuper),
         "super.__new__(X): X is not 'super'");
  Super self(&scope, runtime->newSuper());
  Object type(&scope, args.get(1));
  Object type_or_obj(&scope, args.get(2));
  Frame* frame = thread->currentFrame();
  // frame is for _super_ctor, previous frame is caller of super()
  DCHECK(!frame->isSentinel(), "_super_ctor must have a frame");
  return superInit(thread, self, type, type_or_obj, frame->previousFrame());
}

RawObject FUNC(_builtins, _traceback_frame_get)(Thread* thread,
                                                Arguments args) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isTraceback()) {
    return raiseRequiresFromCaller(thread, args, ID(traceback));
  }
  Traceback self(&scope, *self_obj);
  Object function(&scope, self.function());
  Object lasti(&scope, self.lasti());
  FrameProxy new_frame(&scope, runtime->newFrameProxy(thread, function, lasti));
  // TODO(T91250285): Figure out a way to initialize the frame's locals dict
  new_frame.setLocals(runtime->newDict());
  return *new_frame;
}

RawObject FUNC(_builtins, _traceback_lineno_get)(Thread* thread,
                                                 Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isTraceback()) {
    return raiseRequiresFromCaller(thread, args, ID(traceback));
  }
  Traceback self(&scope, *self_obj);
  Object lineno(&scope, self.lineno());
  if (!lineno.isNoneType()) {
    return *lineno;
  }

  Function function(&scope, self.function());
  Object code_obj(&scope, function.code());
  if (code_obj.isCode()) {
    Code code(&scope, *code_obj);
    if (!code.isNative() && code.lnotab().isBytes()) {
      word lasti = SmallInt::cast(self.lasti()).value();
      lineno = SmallInt::fromWord(code.offsetToLineNum(lasti));
      self.setLineno(*lineno);
    }
  }
  return *lineno;
}

RawObject FUNC(_builtins, _traceback_next_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isTraceback()) {
    return raiseRequiresFromCaller(thread, args, ID(traceback));
  }
  Traceback self(&scope, *self_obj);
  return self.next();
}

RawObject FUNC(_builtins, _traceback_next_set)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!self_obj.isTraceback()) {
    return raiseRequiresFromCaller(thread, args, ID(traceback));
  }
  Traceback self(&scope, *self_obj);
  Object next(&scope, args.get(1));
  if (next.isNoneType()) {
    self.setNext(NoneType::object());
    return NoneType::object();
  }
  if (!next.isTraceback()) {
    return thread->raiseWithFmt(LayoutId::kTypeError,
                                "expected traceback object, got '%T", &next);
  }

  Object cursor(&scope, *next);
  while (cursor.isTraceback()) {
    if (cursor == self) {
      return thread->raiseWithFmt(LayoutId::kValueError,
                                  "traceback loop detected");
    }
    cursor = Traceback::cast(*cursor).next();
  }
  DCHECK(cursor.isNoneType(), "tb_next should be a traceback or None");
  self.setNext(*next);
  return NoneType::object();
}

RawObject FUNC(_builtins, _tuple_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfTuple(args.get(0)));
}

RawObject FUNC(_builtins, _tuple_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isTuple());
}

RawObject FUNC(_builtins, _tuple_getitem)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfTuple(*self_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(tuple));
  }
  Object key(&scope, args.get(1));
  if (runtime->isInstanceOfInt(*key)) {
    word index = intUnderlying(*key).asWordSaturated();
    if (!SmallInt::isValid(index)) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "cannot fit '%T' into an index-sized integer",
                                  &key);
    }
    Tuple self(&scope, tupleUnderlying(*self_obj));
    word length = self.length();
    if (index < 0) {
      index += length;
    }
    if (index < 0 || index >= length) {
      return thread->raiseWithFmt(LayoutId::kIndexError,
                                  "tuple index out of range");
    }
    return self.at(index);
  }

  word start, stop;
  if (!tryUnpackSlice(key, &start, &stop)) {
    return Unbound::object();
  }

  Tuple self(&scope, tupleUnderlying(*self_obj));
  word length = self.length();
  word result_len = Slice::adjustIndices(length, &start, &stop, 1);
  if (result_len == length) {
    return *self;
  }
  return runtime->tupleSubseq(thread, self, start, result_len);
}

RawObject FUNC(_builtins, _tuple_getslice)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Tuple self(&scope, tupleUnderlying(args.get(0)));
  word start = SmallInt::cast(args.get(1)).value();
  word stop = SmallInt::cast(args.get(2)).value();
  word step = SmallInt::cast(args.get(3)).value();
  return tupleSlice(thread, self, start, stop, step);
}

RawObject FUNC(_builtins, _tuple_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfTuple(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(tuple));
}

RawObject FUNC(_builtins, _tuple_len)(Thread*, Arguments args) {
  return SmallInt::fromWord(tupleUnderlying(args.get(0)).length());
}

RawObject FUNC(_builtins, _tuple_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  DCHECK(type != runtime->typeAt(LayoutId::kTuple), "cls must not be tuple");
  DCHECK(args.get(1).isTuple(), "old_tuple must be exact tuple");
  Layout layout(&scope, type.instanceLayout());
  UserTupleBase instance(&scope, runtime->newInstance(layout));
  instance.setValue(args.get(1));
  return *instance;
}

RawObject FUNC(_builtins, _type)(Thread* thread, Arguments args) {
  return thread->runtime()->typeOf(args.get(0));
}

RawObject FUNC(_builtins, _type_ctor)(Thread* thread, Arguments args) {
  DCHECK(args.get(0) == thread->runtime()->typeAt(LayoutId::kType),
         "expected cls==type");
  return thread->runtime()->typeOf(args.get(1));
}

RawObject FUNC(_builtins, _type_abstractmethods_del)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  if (type.abstractMethods().isUnbound()) {
    Object name(&scope,
                thread->runtime()->symbols()->at(ID(__abstractmethods__)));
    return thread->raise(LayoutId::kAttributeError, *name);
  }
  type.setAbstractMethods(Unbound::object());
  type.setFlagsAndBuiltinBase(
      static_cast<Type::Flag>(type.flags() & ~Type::Flag::kIsAbstract),
      type.builtinBase());
  return NoneType::object();
}

RawObject FUNC(_builtins, _type_abstractmethods_get)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  Object methods(&scope, type.abstractMethods());
  if (!methods.isUnbound()) {
    return *methods;
  }
  Object name(&scope,
              thread->runtime()->symbols()->at(ID(__abstractmethods__)));
  return thread->raise(LayoutId::kAttributeError, *name);
}

RawObject FUNC(_builtins, _type_abstractmethods_set)(Thread* thread,
                                                     Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  Object abstract(&scope, Interpreter::isTrue(thread, args.get(1)));
  if (abstract.isError()) return *abstract;
  type.setAbstractMethods(args.get(1));
  if (Bool::cast(*abstract).value()) {
    type.setFlagsAndBuiltinBase(
        static_cast<Type::Flag>(type.flags() | Type::Flag::kIsAbstract),
        type.builtinBase());
  }
  return NoneType::object();
}

RawObject FUNC(_builtins, _type_bases_del)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  Str name(&scope, strUnderlying(type.name()));
  return thread->raiseWithFmt(LayoutId::kTypeError, "can't delete %S.__bases__",
                              &name);
}

RawObject FUNC(_builtins, _type_bases_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  if (!thread->runtime()->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  return type.bases();
}

RawObject FUNC(_builtins, _type_bases_set)(Thread*, Arguments) {
  UNIMPLEMENTED("type.__bases__ setter");
}

RawObject FUNC(_builtins, _type_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfType(args.get(0)));
}

RawObject FUNC(_builtins, _type_check_exact)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isType());
}

RawObject FUNC(_builtins, _type_dunder_call)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object self_obj(&scope, args.get(0));
  Tuple pargs(&scope, args.get(1));
  Dict kwargs(&scope, args.get(2));
  word pargs_length = pargs.length();
  bool is_kwargs_empty = kwargs.numItems() == 0;
  // Shortcut for type(x) calls.
  if (pargs_length == 1 && is_kwargs_empty &&
      self_obj == runtime->typeAt(LayoutId::kType)) {
    return runtime->typeOf(pargs.at(0));
  }

  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "'__call__' for '%Y' objects doesn't apply to a '%T' object", ID(type),
        &self_obj);
  }
  Type self(&scope, *self_obj);

  // `instance = self.__new__(...)`
  Object instance(&scope, NoneType::object());
  Object call_args_obj(&scope, NoneType::object());
  bool use_object_dunder_new =
      self.isType() && self.hasFlag(Type::Flag::kHasObjectDunderNew);
  if (use_object_dunder_new) {
    // Most common case `__new__` was not overridden and is just
    // `object.__new__`.
    instance = objectNew(thread, self);
    if (instance.isErrorException()) return *instance;
  } else {
    Object dunder_new(&scope, Unbound::object());
    Object dunder_new_name(&scope, runtime->symbols()->at(ID(__new__)));
    if (self.isType()) {
      // Metaclass is "type" so we do not need to check for __new__ being a
      // datadescriptor and we can look it up directly on the type.
      dunder_new = typeLookupInMro(thread, *self, *dunder_new_name);
    }
    if (dunder_new.isStaticMethod()) {
      // Next most common case `__new__` is overridden with a normal function
      dunder_new = StaticMethod::cast(*dunder_new).function();
    } else {
      // Finally fallback to complete lookup for corner cases
      dunder_new = typeGetAttribute(thread, self, dunder_new_name);
    }

    CHECK(!dunder_new.isError(), "self must have __new__");
    thread->stackPush(*dunder_new);
    if (is_kwargs_empty) {
      thread->stackPush(*self);
      for (word i = 0; i < pargs_length; ++i) {
        thread->stackPush(pargs.at(i));
      }
      instance = Interpreter::call(thread, pargs_length + 1);
    } else {
      MutableTuple call_args(&scope,
                             runtime->newMutableTuple(pargs_length + 1));
      call_args.atPut(0, *self);
      call_args.replaceFromWith(1, *pargs, pargs_length);
      thread->stackPush(call_args.becomeImmutable());
      thread->stackPush(*kwargs);
      instance = Interpreter::callEx(thread, CallFunctionExFlag::VAR_KEYWORDS);
      call_args_obj = *call_args;
    }
    if (instance.isErrorException()) return *instance;
    if (!typeIsSubclass(runtime->typeOf(*instance), *self)) {
      return *instance;
    }
  }

  // instance.__init__(...)
  Object dunder_init_name(&scope, runtime->symbols()->at(ID(__init__)));
  Object dunder_init(&scope, typeGetAttribute(thread, self, dunder_init_name));
  // `object.__init__` does nothing, we may be able to just skip things.
  // The exception to the rule being `object.__init__` raising errors when
  // arguments are provided and nothing is overridden.
  if (dunder_init != runtime->objectDunderInit() ||
      (use_object_dunder_new &&
       (pargs.length() != 0 || kwargs.numItems() != 0))) {
    CHECK(!dunder_init.isError(), "self must have __init__");
    Object result(&scope, NoneType::object());
    thread->stackPush(*dunder_init);
    if (is_kwargs_empty) {
      thread->stackPush(*instance);
      for (word i = 0; i < pargs_length; ++i) {
        thread->stackPush(pargs.at(i));
      }
      result = Interpreter::call(thread, pargs_length + 1);
    } else {
      if (!call_args_obj.isMutableTuple()) {
        MutableTuple call_args(&scope,
                               runtime->newMutableTuple(pargs_length + 1));
        call_args.atPut(0, *instance);
        call_args.replaceFromWith(1, *pargs, pargs_length);
        call_args_obj = *call_args;
      } else {
        MutableTuple::cast(*call_args_obj).atPut(0, *instance);
      }
      thread->stackPush(*call_args_obj);
      thread->stackPush(*kwargs);
      result = Interpreter::callEx(thread, CallFunctionExFlag::VAR_KEYWORDS);
    }
    if (result.isErrorException()) return *result;
    if (!result.isNoneType()) {
      Object type_name(&scope, self.name());
      return thread->raiseWithFmt(LayoutId::kTypeError,
                                  "%S.__init__ returned non None", &type_name);
    }
  }
  return *instance;
}

RawObject FUNC(_builtins, _type_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfType(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(type));
}

RawObject FUNC(_builtins, _type_issubclass)(Thread*, Arguments args) {
  return Bool::fromBool(typeIsSubclass(args.get(0), args.get(1)));
}

bool FUNC(_builtins, _type_issubclass_intrinsic)(Thread* thread) {
  RawObject subclass = thread->stackPeek(1);
  RawObject superclass = thread->stackPeek(0);
  thread->stackDrop(2);
  thread->stackSetTop(Bool::fromBool(typeIsSubclass(subclass, superclass)));
  return true;
}

RawObject FUNC(_builtins, _type_module_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  // If this is called on Type itself, typeAtById won't find __module__, and
  // the type isn't a CPython heap type, but it should still return builtins
  if (type.isBuiltin() && type.builtinBase() == LayoutId::kType) {
    return runtime->symbols()->at(ID(builtins));
  }
  Object result(&scope, typeAtById(thread, type, ID(__module__)));
  if (result.isErrorNotFound()) {
    if (!type.isCPythonHeaptype()) {
      return runtime->symbols()->at(ID(builtins));
    }
    Object name(&scope, runtime->symbols()->at(ID(__module__)));
    return objectRaiseAttributeError(thread, type, name);
  }
  return *result;
}

RawObject FUNC(_builtins, _type_module_set)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  Object value(&scope, args.get(1));
  if (!type.hasMutableDict()) {
    return raiseTypeErrorCannotSetImmutable(thread, type);
  }
  typeAtPutById(thread, type, ID(__module__), value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _type_name_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  return type.name();
}

RawObject FUNC(_builtins, _type_name_set)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  if (!type.hasMutableDict()) {
    return raiseTypeErrorCannotSetImmutable(thread, type);
  }

  Object value(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*value)) {
    Object type_name(&scope, type.name());
    return thread->raiseWithFmt(
        LayoutId::kTypeError, "can only assign string to %S.__name__, not '%T'",
        &type_name, &value);
  }
  type.setName(*value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _type_new)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type metaclass(&scope, args.get(0));
  Str name(&scope, strUnderlying(args.get(1)));
  Tuple bases(&scope, tupleUnderlying(args.get(2)));
  Dict dict(&scope, args.get(3));
  Bool is_heaptype(&scope, args.get(4));

  word flags = Type::Flag::kIsBasetype;
  flags |=
      is_heaptype.value() ? Type::Flag::kIsCPythonHeaptype : Type::Flag::kNone;
  return typeNew(thread, metaclass, name, bases, dict, flags,
                 /*inherit_slots=*/true, /*add_instance_dict=*/true);
}

RawObject FUNC(_builtins, _type_proxy)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Type type(&scope, args.get(0));
  if (type.proxy().isNoneType()) {
    type.setProxy(thread->runtime()->newTypeProxy(type));
  }
  return type.proxy();
}

RawObject FUNC(_builtins, _type_proxy_check)(Thread*, Arguments args) {
  return Bool::fromBool(args.get(0).isTypeProxy());
}

RawObject FUNC(_builtins, _type_proxy_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  TypeProxy self(&scope, args.get(0));
  Object name(&scope, args.get(1));
  name = attributeName(thread, name);
  if (name.isErrorException()) return *name;
  Object default_obj(&scope, args.get(2));
  Type type(&scope, self.type());
  Object result(&scope, typeAt(type, name));
  if (result.isError()) {
    return *default_obj;
  }
  return *result;
}

RawObject FUNC(_builtins, _type_proxy_guard)(Thread* thread, Arguments args) {
  if (args.get(0).isTypeProxy()) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(type_proxy));
}

RawObject FUNC(_builtins, _type_proxy_keys)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  TypeProxy self(&scope, args.get(0));
  Type type(&scope, self.type());
  return typeKeys(thread, type);
}

RawObject FUNC(_builtins, _type_proxy_len)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  TypeProxy self(&scope, args.get(0));
  Type type(&scope, self.type());
  return SmallInt::fromWord(typeLen(thread, type));
}

RawObject FUNC(_builtins, _type_proxy_values)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  TypeProxy self(&scope, args.get(0));
  Type type(&scope, self.type());
  return typeValues(thread, type);
}

RawObject FUNC(_builtins, _type_qualname_get)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  return type.qualname();
}

RawObject FUNC(_builtins, _type_qualname_set)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfType(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(type));
  }
  Type type(&scope, *self_obj);
  if (!type.hasMutableDict()) {
    return raiseTypeErrorCannotSetImmutable(thread, type);
  }

  Object value(&scope, args.get(1));
  if (!runtime->isInstanceOfStr(*value)) {
    Object type_name(&scope, type.name());
    return thread->raiseWithFmt(
        LayoutId::kTypeError,
        "can only assign string to %S.__qualname__, not '%T'", &type_name,
        &value);
  }
  type.setQualname(*value);
  return NoneType::object();
}

RawObject FUNC(_builtins, _type_subclass_guard)(Thread* thread,
                                                Arguments args) {
  RawObject subclass_obj = args.get(0);
  if (!thread->runtime()->isInstanceOfType(subclass_obj)) {
    return raiseRequiresFromCaller(thread, args, ID(type));
  }
  RawObject superclass_obj = args.get(1);
  if (typeIsSubclass(subclass_obj, superclass_obj)) {
    return NoneType::object();
  }
  HandleScope scope(thread);
  Type subclass(&scope, subclass_obj);
  Type superclass(&scope, superclass_obj);
  Function function(&scope,
                    thread->currentFrame()->previousFrame()->function());
  Str function_name(&scope, function.name());
  Str subclass_name(&scope, subclass.name());
  Str superclass_name(&scope, superclass.name());
  return thread->raiseWithFmt(LayoutId::kTypeError,
                              "'%S': '%S' is not a subclass of '%S'",
                              &function_name, &subclass_name, &superclass_name);
}

RawObject FUNC(_builtins, _unimplemented)(Thread* thread, Arguments) {
  HandleScope scope(thread);

  // Environment override?
  const char* pyro_raise_on_unimplemented =
      std::getenv("PYRO_RAISE_ON_UNIMPLEMENTED");
  bool raise_instead_of_abort =
      (pyro_raise_on_unimplemented != nullptr &&
       ::strcmp(pyro_raise_on_unimplemented, "1") == 0);

  // If sys.PYRO_RAISE_ON_UNIMPLEMENTED is set to a true value
  if (!raise_instead_of_abort) {
    Object sys_dot_pyro_raise_on_unimplemented(
        &scope, thread->runtime()->lookupNameInModule(
                    thread, ID(sys), ID(PYRO_RAISE_ON_UNIMPLEMENTED)));
    if (!sys_dot_pyro_raise_on_unimplemented.isError()) {
      Object o(&scope, Interpreter::isTrue(
                           thread, *sys_dot_pyro_raise_on_unimplemented));
      raise_instead_of_abort = (!o.isError()) && (*o == Bool::trueObj());
    }
  }

  if (raise_instead_of_abort) {
    return thread->raiseWithFmt(LayoutId::kNotImplementedError,
                                "overrode _unimplemented abort");
  }
  thread->runtime()->printTraceback(thread, File::kStderr);

  // Attempt to identify the calling function.
  Object function_obj(&scope,
                      thread->currentFrame()->previousFrame()->function());
  if (!function_obj.isError()) {
    Function function(&scope, *function_obj);
    Str function_name(&scope, function.name());
    unique_c_ptr<char> name_cstr(function_name.toCStr());
    fprintf(stderr, "\n'_unimplemented' called in function '%s'\n",
            name_cstr.get());
  } else {
    fputs("\n'_unimplemented' called.\n", stderr);
  }
  fputs(
      "\nuse env PYRO_RAISE_ON_UNIMPLEMENTED=1 or"
      "\nsys.PYRO_RAISE_ON_UNIMPLEMENTED=True to raise instead of abort.\n",
      stderr);

  std::abort();
}

RawObject FUNC(_builtins, _warn)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object message(&scope, args.get(0));
  Object category(&scope, args.get(1));
  Object stacklevel(&scope, args.get(2));
  Object source(&scope, args.get(3));
  return thread->invokeFunction4(ID(warnings), ID(warn), message, category,
                                 stacklevel, source);
}

RawObject FUNC(_builtins, _weakref_callback)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfWeakRef(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(weakref));
  }
  WeakRef self(&scope, weakRefUnderlying(*self_obj));
  Object callback(&scope, self.callback());
  if (callback.isNoneType()) {
    return *callback;
  }
  return BoundMethod::cast(*callback).function();
}

RawObject FUNC(_builtins, _weakref_check)(Thread* thread, Arguments args) {
  return Bool::fromBool(thread->runtime()->isInstanceOfWeakRef(args.get(0)));
}

RawObject FUNC(_builtins, _weakref_guard)(Thread* thread, Arguments args) {
  if (thread->runtime()->isInstanceOfWeakRef(args.get(0))) {
    return NoneType::object();
  }
  return raiseRequiresFromCaller(thread, args, ID(weakref));
}

RawObject FUNC(_builtins, _weakref_referent)(Thread* thread, Arguments args) {
  HandleScope scope(thread);
  Object self_obj(&scope, args.get(0));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfWeakRef(*self_obj)) {
    return thread->raiseRequiresType(self_obj, ID(weakref));
  }
  WeakRef self(&scope, weakRefUnderlying(*self_obj));
  return self.referent();
}

}  // namespace py
