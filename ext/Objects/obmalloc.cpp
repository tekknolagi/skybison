// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "cpython-func.h"

#include "api-handle.h"
#include "extension-object.h"
#include "linked-list.h"
#include "runtime.h"

namespace py {

PY_EXPORT int _PyObject_DebugMallocStats(FILE* /*out*/) {
  // A return value of 0 means no debugging hooks are installed.
  return 0;
}

PY_EXPORT void* PyObject_Malloc(size_t size) {
  ListEntry* entry =
      static_cast<ListEntry*>(PyMem_RawMalloc(sizeof(ListEntry) + size));
  entry->prev = nullptr;
  entry->next = nullptr;
  return reinterpret_cast<void*>(entry + 1);
}

PY_EXPORT void PyMem_Del(void* ptr) { return PyMem_RawFree(ptr); }

PY_EXPORT void* PyObject_Calloc(size_t nelem, size_t size) {
  if (size == 0 || nelem == 0) {
    nelem = 1;
    size = 1;
  }
  void* buffer = PyObject_Malloc(nelem * size);
  std::memset(buffer, 0, nelem * size);
  return buffer;
}

PY_EXPORT void* PyObject_Realloc(void* ptr, size_t size) {
  if (ptr == nullptr) return PyObject_Malloc(size);
  ListEntry* entry = static_cast<ListEntry*>(ptr) - 1;
  Runtime* runtime = Thread::current()->runtime();
  bool removed = untrackExtensionObject(runtime, entry);
  entry = static_cast<ListEntry*>(
      PyMem_RawRealloc(entry, sizeof(ListEntry) + size));
  entry->prev = nullptr;
  entry->next = nullptr;
  if (removed) {
    trackExtensionObject(runtime, entry);
  }
  return reinterpret_cast<void*>(entry + 1);
}

PY_EXPORT void PyObject_Free(void* ptr) {
  if (ptr == nullptr) return;
  ListEntry* entry = static_cast<ListEntry*>(ptr) - 1;
  Runtime* runtime = Thread::current()->runtime();
  bool removed = untrackExtensionObject(runtime, entry);
  if (removed) {
    // Set native pointer to `None` to signal the `finalizeExtensionObject` code
    // that the object memory was freed.
    PyObject* obj = reinterpret_cast<PyObject*>(ptr);
    ApiHandle::asNativeProxy(ApiHandle::fromPyObject(obj)).setNative(NoneType::object());
  }
  return PyMem_RawFree(entry);
}

PY_EXPORT void* PyMem_Malloc(size_t size) { return PyMem_RawMalloc(size); }

PY_EXPORT void* PyMem_Calloc(size_t nelem, size_t size) {
  return PyMem_RawCalloc(nelem, size);
}

PY_EXPORT void* PyMem_Realloc(void* ptr, size_t size) {
  return PyMem_RawRealloc(ptr, size);
}

PY_EXPORT void PyMem_Free(void* ptr) { return PyMem_RawFree(ptr); }

PY_EXPORT void* PyMem_RawMalloc(size_t size) {
  if (size == 0) {
    size = 1;
  }
  return std::malloc(size);
}

PY_EXPORT void* PyMem_RawCalloc(size_t nelem, size_t size) {
  if (size == 0 || nelem == 0) {
    nelem = 1;
    size = 1;
  }
  return std::calloc(nelem, size);
}

PY_EXPORT void* PyMem_RawRealloc(void* ptr, size_t size) {
  if (size == 0) {
    size = 1;
  }
  return std::realloc(ptr, size);
}

PY_EXPORT void PyMem_RawFree(void* ptr) { return std::free(ptr); }

PY_EXPORT void* PyMem_New_Func(size_t size, size_t n) {
  if (n > kMaxWord / size) return nullptr;
  return PyMem_Malloc(n * size);
}

PY_EXPORT char* _PyMem_RawStrdup(const char* str) {
  size_t size = std::strlen(str) + 1;
  char* result = static_cast<char*>(PyMem_RawMalloc(size));
  if (result != nullptr) {
    std::memcpy(result, str, size);
  }
  return result;
}

PY_EXPORT char* _PyMem_Strdup(const char* str) {
  size_t size = std::strlen(str) + 1;
  char* result = static_cast<char*>(PyMem_Malloc(size));
  if (result != nullptr) {
    std::memcpy(result, str, size);
  }
  return result;
}

}  // namespace py
