// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "api-handle.h"
#include "runtime.h"

namespace py {

PY_EXPORT PyObject* PyClassMethod_New(PyObject* callable) {
  DCHECK(callable != nullptr, "uninitialized staticmethod object");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object callable_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(callable)));
  ClassMethod result(&scope, runtime->newClassMethod());
  result.setFunction(*callable_obj);
  return ApiHandle::newReferenceWithManaged(runtime, *result);
}

PY_EXPORT PyObject* PyStaticMethod_New(PyObject* callable) {
  DCHECK(callable != nullptr, "uninitialized classmethod object");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object callable_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(callable)));
  StaticMethod result(&scope, runtime->newStaticMethod());
  result.setFunction(*callable_obj);
  return ApiHandle::newReferenceWithManaged(runtime, *result);
}

PY_EXPORT PyTypeObject* PyClassMethod_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kClassMethod)));
}

PY_EXPORT PyTypeObject* PyFunction_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kFunction)));
}

PY_EXPORT PyTypeObject* PyStaticMethod_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kStaticMethod)));
}

PY_EXPORT PyObject* _PyCFunction_FastCallDict(PyObject* /* c */,
                                              PyObject* const* /* s */,
                                              Py_ssize_t /* s */,
                                              PyObject* /* s */) {
  UNIMPLEMENTED("_PyCFunction_FastCallDict");
}

PY_EXPORT PyObject* _PyCFunction_FastCallKeywords(PyObject* /* c */,
                                                  PyObject* const* /* s */,
                                                  Py_ssize_t /* s */,
                                                  PyObject* /* s */) {
  UNIMPLEMENTED("_PyCFunction_FastCallKeywords");
}

PY_EXPORT PyObject* _PyFunction_FastCallDict(PyObject* /* c */,
                                             PyObject* const* /* s */,
                                             Py_ssize_t /* s */,
                                             PyObject* /* s */) {
  UNIMPLEMENTED("_PyFunction_FastCallDict");
}

PY_EXPORT PyObject* _PyFunction_FastCallKeywords(PyObject* /* c */,
                                                 PyObject* const* /* k */,
                                                 Py_ssize_t /* s */,
                                                 PyObject* /* s */) {
  UNIMPLEMENTED("_PyFunction_FastCallKeywords");
}

}  // namespace py
