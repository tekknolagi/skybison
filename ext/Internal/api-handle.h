/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once

#include <cstdint>

#include "cpython-types.h"

#include "handles.h"
#include "objects.h"

namespace py {

class PointerVisitor;

static const Py_ssize_t kImmediateRefcnt = Py_ssize_t{1} << 63;

class ApiHandle : public PyObject {
 public:
  // Returns a handle for a managed object.  Increments the reference count of
  // the handle.
  static ApiHandle* newReference(Runtime* runtime, RawObject obj);

  // Returns a handle for a managed object. This must not be called with an
  // extension object or an object for which `isEncodeableAsImmediate` is true.
  static ApiHandle* newReferenceWithManaged(Runtime* runtime, RawObject obj);

  // Returns a handle for a managed object.  Does not affect the reference count
  // of the handle.
  static ApiHandle* borrowedReference(Runtime* runtime, RawObject obj);

  static ApiHandle* handleFromImmediate(RawObject obj);

  // Returns the managed object associated with the handle.  Decrements the
  // reference count of handle.
  static RawObject stealReference(PyObject* py_obj);

  // Returns the managed object associated with the handle checking for
  static RawObject checkFunctionResult(Thread* thread, PyObject* result);

  static ApiHandle* fromPyObject(PyObject* py_obj);

  static ApiHandle* fromPyTypeObject(PyTypeObject* type);

  // Get the object from the handle's reference field.
  static RawObject asObject(ApiHandle* handle);

  static RawObject asObjectImmediate(ApiHandle* handle);

  static RawObject asObjectNoImmediate(ApiHandle* handle);

  // Return native proxy belonging to an extension object.
  static RawNativeProxy asNativeProxy(ApiHandle* handle);

  // Each ApiHandle can have one pointer to cached data, which will be freed
  // when the handle is destroyed.
  static void* cache(Runtime* runtime, ApiHandle* handle);
  static void setCache(Runtime* runtime, ApiHandle* handle, void* value);

  // Decrements the reference count of the handle to signal the removal of a
  // reference count from extension code.
  static void decref(ApiHandle* handle);

  static void decrefNoImmediate(ApiHandle* handle);

  // Remove the ApiHandle from the dictionary and free its memory
  static void dispose(ApiHandle* handle);
  static void disposeWithRuntime(Runtime* runtime, ApiHandle* handle);

  static bool isImmediate(ApiHandle* handle);

  // Increments the reference count of the handle to signal the addition of a
  // reference from extension code.
  static void incref(ApiHandle* handle);

  static void increfNoImmediate(ApiHandle* handle);

  // Returns the number of references to this handle from extension code.
  static Py_ssize_t refcnt(ApiHandle* handle);

  static Py_ssize_t refcntNoImmediate(ApiHandle* handle);

  static void setRefcnt(ApiHandle* handle, Py_ssize_t count);

  static void setBorrowedNoImmediate(ApiHandle* handle);
  static bool isBorrowedNoImmediate(ApiHandle* handle);

 private:
  static bool isEncodeableAsImmediate(RawObject obj);

  static const Py_ssize_t kBorrowedBit = Py_ssize_t{1} << 63;

  static const long kImmediateTag = 0x1;
  static const long kImmediateMask = 0x7;

  static_assert(kBorrowedBit == kImmediateRefcnt,
                "keep kBorrowedBit and kImmediateRefcnt in sync");
  static_assert(kImmediateMask < alignof(PyObject*),
                "Stronger alignment guarantees are required for immediate "
                "tagged PyObject* to work.");

  DISALLOW_IMPLICIT_CONSTRUCTORS(ApiHandle);
};

static_assert(sizeof(ApiHandle) == sizeof(PyObject),
              "ApiHandle must not add members to PyObject");

struct FreeListNode {
  FreeListNode* next;
};

static_assert(sizeof(FreeListNode) <= sizeof(ApiHandle),
              "Free ApiHandle should be usable as a FreeListNode");

inline RawObject ApiHandle::asObject(ApiHandle* handle) {
  if (isImmediate(handle)) return asObjectImmediate(handle);
  return asObjectNoImmediate(handle);
}

inline RawObject ApiHandle::asObjectImmediate(ApiHandle* handle) {
  DCHECK(isImmediate(handle), "expected immediate");
  return RawObject{reinterpret_cast<uword>(handle) ^ kImmediateTag};
}

inline RawObject ApiHandle::asObjectNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  return RawObject{handle->reference_};
}

inline void ApiHandle::decref(ApiHandle* handle) {
  if (isImmediate(handle)) return;
  decrefNoImmediate(handle);
}

inline void ApiHandle::decrefNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  DCHECK((handle->ob_refcnt & ~kBorrowedBit) > 0, "reference count underflow");
  --handle->ob_refcnt;
  // Dispose `ApiHandle`s without `kBorrowedBit` when they reach refcount zero.
  if (handle->ob_refcnt == 0) {
    dispose(handle);
  }
}

inline ApiHandle* ApiHandle::fromPyObject(PyObject* py_obj) {
  return static_cast<ApiHandle*>(py_obj);
}

inline ApiHandle* ApiHandle::fromPyTypeObject(PyTypeObject* type) {
  return fromPyObject(reinterpret_cast<PyObject*>(type));
}

inline ApiHandle* ApiHandle::handleFromImmediate(RawObject obj) {
  DCHECK(isEncodeableAsImmediate(obj), "expected immediate");
  return reinterpret_cast<ApiHandle*>(obj.raw() ^ kImmediateTag);
}

inline void ApiHandle::incref(ApiHandle* handle) {
  if (isImmediate(handle)) return;
  // fprintf(stderr, "incref(%p)\n", (void*)this);
  increfNoImmediate(handle);
}

inline void ApiHandle::increfNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  DCHECK((handle->ob_refcnt & ~kBorrowedBit) <
             (std::numeric_limits<Py_ssize_t>::max() & ~kBorrowedBit),
         "Reference count overflowed");
  ++handle->ob_refcnt;
}

inline bool ApiHandle::isImmediate(ApiHandle* handle) {
  return (reinterpret_cast<uword>(handle) & kImmediateMask) != 0;
}

inline Py_ssize_t ApiHandle::refcnt(ApiHandle* handle) {
  if (isImmediate(handle)) return kImmediateRefcnt;
  return refcntNoImmediate(handle);
}

inline Py_ssize_t ApiHandle::refcntNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  return handle->ob_refcnt & ~kBorrowedBit;
}

inline void ApiHandle::setBorrowedNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  handle->ob_refcnt |= kBorrowedBit;
}

inline bool ApiHandle::isBorrowedNoImmediate(ApiHandle* handle) {
  DCHECK(!isImmediate(handle), "must not be called with immediate object");
  return (handle->ob_refcnt & kBorrowedBit) != 0;
}

inline RawObject ApiHandle::stealReference(PyObject* py_obj) {
  ApiHandle* handle = ApiHandle::fromPyObject(py_obj);
  if (isImmediate(handle)) return asObjectImmediate(handle);
  DCHECK((handle->ob_refcnt & ~kBorrowedBit) > 0, "refcount underflow");
  // Mark stolen reference as borrowed. This is to support code like this that
  // increases refcount after the fact:
  //     PyModule_AddObject(..., x);
  //     Py_INCREF(x);
  handle->ob_refcnt |= kBorrowedBit;
  handle->ob_refcnt--;
  return asObjectNoImmediate(handle);
}

}  // namespace py
