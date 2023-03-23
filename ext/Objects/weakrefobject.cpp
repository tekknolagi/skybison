// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "api-handle.h"
#include "runtime.h"

namespace py {

PY_EXPORT int PyWeakref_Check_Func(PyObject* obj) {
  return ApiHandle::asObject(ApiHandle::fromPyObject(obj)).isWeakRef();
}

PY_EXPORT void PyObject_ClearWeakRefs(PyObject* /* obj */) {
  // Do nothing and delegated to the garbage collector.
}

PY_EXPORT PyObject* PyWeakref_GET_OBJECT_Func(PyObject* ref) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  // ref is assumed to be a WeakRef already
  WeakRef weakref(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(ref)));
  return ApiHandle::borrowedReference(thread->runtime(), weakref.referent());
}

PY_EXPORT PyObject* PyWeakref_GetObject(PyObject* ref) {
  Thread* thread = Thread::current();
  if (ref == nullptr) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "PyWeakref_GetObject expected non-null ref");
    return nullptr;
  }
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(ref)));
  if (!obj.isWeakRef()) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "PyWeakref_GetObject expected weakref");
    return nullptr;
  }
  WeakRef weakref(&scope, *obj);
  return ApiHandle::borrowedReference(thread->runtime(), weakref.referent());
}

PY_EXPORT PyObject* PyWeakref_NewProxy(PyObject* ob, PyObject* callback) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object referent(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(ob)));
  Object callback_obj(&scope, NoneType::object());
  if (callback != nullptr) {
    callback_obj = ApiHandle::asObject(ApiHandle::fromPyObject(callback));
  }
  Object result_obj(&scope, thread->invokeFunction2(ID(_weakref), ID(proxy),
                                                    referent, callback_obj));
  if (result_obj.isError()) {
    return nullptr;
  }
  return ApiHandle::newReferenceWithManaged(thread->runtime(), *result_obj);
}

PY_EXPORT PyObject* PyWeakref_NewRef(PyObject* obj, PyObject* callback) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object referent(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  Object callback_obj(&scope, NoneType::object());
  if (callback != nullptr) {
    callback_obj = ApiHandle::asObject(ApiHandle::fromPyObject(callback));
  }
  Runtime* runtime = thread->runtime();
  WeakRef ref(&scope, runtime->newWeakRef(thread, referent));
  if (callback_obj.isNoneType()) {
    ref.setCallback(*callback_obj);
    return ApiHandle::newReferenceWithManaged(runtime, *ref);
  }
  if (runtime->isCallable(thread, callback_obj)) {
    ref.setCallback(runtime->newBoundMethod(callback_obj, ref));
    return ApiHandle::newReferenceWithManaged(runtime, *ref);
  }
  thread->raiseWithFmt(LayoutId::kTypeError, "callback is not callable");
  return nullptr;
}

}  // namespace py
