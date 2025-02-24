// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
// unicodeobject.c implementation
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <cwchar>

#include "cpython-data.h"
#include "cpython-func.h"

#include "api-handle.h"
#include "bytearray-builtins.h"
#include "bytes-builtins.h"
#include "handles.h"
#include "modules.h"
#include "objects.h"
#include "runtime.h"
#include "str-builtins.h"
#include "unicode.h"
#include "utils.h"

const char* Py_FileSystemDefaultEncoding = "utf-8";
int Py_HasFileSystemDefaultEncoding = 1;
const char* Py_FileSystemDefaultEncodeErrors = "surrogatepass";

namespace py {

typedef byte Py_UCS1;
typedef uint16_t Py_UCS2;

static const int kMaxLongLongChars = 19;  // len(str(2**63-1))
static const int kOverallocateFactor = 4;

PY_EXPORT PyTypeObject* PyUnicodeIter_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(ApiHandle::borrowedReference(
      runtime, runtime->typeAt(LayoutId::kStrIterator)));
}

static RawObject symbolFromError(Thread* thread, const char* error) {
  Runtime* runtime = thread->runtime();
  Symbols* symbols = runtime->symbols();
  if (error == nullptr || std::strcmp(error, "strict") == 0) {
    return symbols->at(ID(strict));
  }
  if (std::strcmp(error, "ignore") == 0) {
    return symbols->at(ID(ignore));
  }
  if (std::strcmp(error, "replace") == 0) {
    return symbols->at(ID(replace));
  }
  return Runtime::internStrFromCStr(thread, error);
}

PY_EXPORT void PyUnicode_WRITE_Func(enum PyUnicode_Kind kind, void* data,
                                    Py_ssize_t index, Py_UCS4 value) {
  if (kind == PyUnicode_1BYTE_KIND) {
    static_cast<Py_UCS1*>(data)[index] = static_cast<Py_UCS1>(value);
  } else if (kind == PyUnicode_2BYTE_KIND) {
    static_cast<Py_UCS2*>(data)[index] = static_cast<Py_UCS2>(value);
  } else {
    DCHECK(kind == PyUnicode_4BYTE_KIND, "kind must be PyUnicode_4BYTE_KIND");
    static_cast<Py_UCS4*>(data)[index] = static_cast<Py_UCS4>(value);
  }
}

PY_EXPORT void _PyUnicodeWriter_Dealloc(_PyUnicodeWriter* writer) {
  PyMem_Free(writer->data);
}

PY_EXPORT PyObject* _PyUnicodeWriter_Finish(_PyUnicodeWriter* writer) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Str str(&scope, runtime->newStrFromUTF32(View<int32_t>(
                      static_cast<int32_t*>(writer->data), writer->pos)));
  PyMem_Free(writer->data);
  return ApiHandle::newReference(runtime, *str);
}

PY_EXPORT void _PyUnicodeWriter_Init(_PyUnicodeWriter* writer) {
  std::memset(writer, 0, sizeof(*writer));
  writer->kind = PyUnicode_4BYTE_KIND;
}

static int _PyUnicodeWriter_PrepareInternal(_PyUnicodeWriter* writer,
                                            Py_ssize_t length,
                                            Py_UCS4 /* maxchar */) {
  writer->maxchar = kMaxUnicode;
  if (length > kMaxWord - writer->pos) {
    Thread::current()->raiseMemoryError();
    return -1;
  }
  Py_ssize_t newlen = writer->pos + length;
  if (writer->data == nullptr) {
    if (writer->overallocate &&
        newlen <= (kMaxWord - newlen / kOverallocateFactor)) {
      // overallocate to limit the number of realloc()
      newlen += newlen / kOverallocateFactor;
    }
    writer->data = PyMem_Malloc(newlen * sizeof(int32_t));
    if (writer->data == nullptr) return -1;
  } else if (newlen > writer->size) {
    if (writer->overallocate &&
        newlen <= (kMaxWord - newlen / kOverallocateFactor)) {
      // overallocate to limit the number of realloc()
      newlen += newlen / kOverallocateFactor;
    }
    writer->data = PyMem_Realloc(writer->data, newlen * sizeof(int32_t));
    if (writer->data == nullptr) return -1;
  }
  writer->size = newlen;
  return 0;
}

PY_EXPORT int _PyUnicodeWriter_Prepare(_PyUnicodeWriter* writer,
                                       Py_ssize_t length, Py_UCS4 maxchar) {
  if (length <= writer->size - writer->pos || length == 0) return 0;
  return _PyUnicodeWriter_PrepareInternal(writer, length, maxchar);
}

PY_EXPORT int _PyUnicodeWriter_WriteASCIIString(_PyUnicodeWriter* writer,
                                                const char* ascii,
                                                Py_ssize_t len) {
  if (len == -1) len = std::strlen(ascii);
  if (writer->data == nullptr && !writer->overallocate) {
    writer->data = PyMem_Malloc(len * sizeof(int32_t));
    writer->size = len;
  }

  if (_PyUnicodeWriter_Prepare(writer, len, kMaxUnicode) == -1) return -1;
  Py_UCS4* data = static_cast<Py_UCS4*>(writer->data);
  for (Py_ssize_t i = 0; i < len; ++i) {
    CHECK(ascii[i] >= 0, "_PyUnicodeWriter_WriteASCIIString only takes ASCII");
    data[writer->pos++] = static_cast<uint8_t>(ascii[i]);
  }
  return 0;
}

PY_EXPORT int _PyUnicodeWriter_WriteCharInline(_PyUnicodeWriter* writer,
                                               Py_UCS4 ch) {
  if (_PyUnicodeWriter_Prepare(writer, 1, ch) < 0) return -1;
  PyUnicode_WRITE(PyUnicode_4BYTE_KIND, writer->data, writer->pos, ch);
  writer->pos++;
  return 0;
}

PY_EXPORT int _PyUnicodeWriter_WriteChar(_PyUnicodeWriter* writer, Py_UCS4 ch) {
  return _PyUnicodeWriter_WriteCharInline(writer, ch);
}

PY_EXPORT int _PyUnicodeWriter_WriteLatin1String(_PyUnicodeWriter* writer,
                                                 const char* str,
                                                 Py_ssize_t len) {
  if (_PyUnicodeWriter_Prepare(writer, len, kMaxUnicode) == -1) return -1;
  Py_UCS4* data = static_cast<Py_UCS4*>(writer->data);
  for (Py_ssize_t i = 0; i < len; ++i) {
    data[writer->pos++] = static_cast<uint8_t>(str[i]);
  }
  return 0;
}

PY_EXPORT int _PyUnicodeWriter_WriteStr(_PyUnicodeWriter* writer,
                                        PyObject* str) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Str src(&scope, strUnderlying(*obj));
  Py_ssize_t codepoints = src.codePointLength();
  if (_PyUnicodeWriter_Prepare(writer, codepoints, kMaxUnicode) == -1) {
    return -1;
  }
  Py_UCS4* data = static_cast<Py_UCS4*>(writer->data);
  for (word i = 0, len = src.length(), cp_len; i < len; i += cp_len) {
    int32_t cp = src.codePointAt(i, &cp_len);
    data[writer->pos++] = cp;
  }
  return 0;
}

PY_EXPORT int _PyUnicodeWriter_WriteSubstring(_PyUnicodeWriter* writer,
                                              PyObject* str, Py_ssize_t start,
                                              Py_ssize_t end) {
  if (end == 0) return 0;
  Py_ssize_t len = end - start;
  if (_PyUnicodeWriter_Prepare(writer, len, kMaxUnicode) < 0) return -1;

  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Str src(&scope, strUnderlying(*obj));
  word start_index = thread->strOffset(src, start);
  DCHECK_BOUND(start_index, src.length());
  word end_index = thread->strOffset(src, end);
  DCHECK_BOUND(end_index, src.length());
  Py_UCS4* data = static_cast<Py_UCS4*>(writer->data);
  for (word i = start_index, cp_len; i < end_index; i += cp_len) {
    int32_t cp = src.codePointAt(i, &cp_len);
    data[writer->pos++] = cp;
  }
  return 0;
}

// Facebook: D13491655
// Most of the following helper functions, along with PyUnicode_FromFormat and
// PyUnicode_FromFormatV are directly imported from CPython. The following
// modifications have been made:
//
// - Since our internal strings are always UTF-8, we don't need maxchar or any
// of the helper functions required to calculate it
//
// - Since our strings are immutable, we can't use PyUnicode_Fill. However,
// since the helper functions always use it to append to strings, we can get
// away with just writing characters in a loop.
//
// - Since our internal strings are always UTF-8, there is no need to check
// a character's 'Kind' before writing it to a string
static int writeStr(_PyUnicodeWriter* writer, PyObject* str, Py_ssize_t width,
                    Py_ssize_t precision) {
  if (PyUnicode_READY(str) == -1) return -1;

  Py_ssize_t length = PyUnicode_GET_LENGTH(str);
  if ((precision == -1 || precision >= length) && width <= length) {
    return _PyUnicodeWriter_WriteStr(writer, str);
  }

  if (precision != -1) length = Py_MIN(precision, length);

  Py_ssize_t arglen = Py_MAX(length, width);
  // Facebook: Our internal strings are always UTF-8, don't need maxchar
  // (D13491655)
  if (_PyUnicodeWriter_Prepare(writer, arglen, 0) == -1) return -1;

  if (width > length) {
    Py_ssize_t fill = width - length;
    // Facebook: Our internal strings are immutable, can't use PyUnicode_Fill
    // (D13491655)
    for (Py_ssize_t i = 0; i < fill; ++i) {
      if (_PyUnicodeWriter_WriteCharInline(writer, ' ') == -1) return -1;
    }
  }
  // Facebook: Since we only have one internal representation, we don't have
  // to worry about changing a string's 'Kind' (D13491655)
  return _PyUnicodeWriter_WriteSubstring(writer, str, 0, length);
}

static int writeCStr(_PyUnicodeWriter* writer, const char* str,
                     Py_ssize_t width, Py_ssize_t precision) {
  Py_ssize_t length = std::strlen(str);
  if (precision != -1) length = Py_MIN(length, precision);
  PyObject* unicode =
      PyUnicode_DecodeUTF8Stateful(str, length, "replace", nullptr);
  if (unicode == nullptr) return -1;

  int res = writeStr(writer, unicode, width, -1);
  Py_DECREF(unicode);
  return res;
}

static const char* writeArg(_PyUnicodeWriter* writer, const char* f,
                            va_list* vargs) {
  const char* p = f;
  f++;
  int zeropad = 0;
  if (*f == '0') {
    zeropad = 1;
    f++;
  }

  // parse the width.precision part, e.g. "%2.5s" => width=2, precision=5
  Py_ssize_t width = -1;
  if (Py_ISDIGIT(static_cast<unsigned>(*f))) {
    width = *f - '0';
    f++;
    while (Py_ISDIGIT(static_cast<unsigned>(*f))) {
      if (width > (kMaxWord - (static_cast<int>(*f) - '0')) / 10) {
        Thread::current()->raiseWithFmt(LayoutId::kValueError, "width too big");
        return nullptr;
      }
      width = (width * 10) + (*f - '0');
      f++;
    }
  }
  Py_ssize_t precision = -1;
  if (*f == '.') {
    f++;
    if (Py_ISDIGIT(static_cast<unsigned>(*f))) {
      precision = (*f - '0');
      f++;
      while (Py_ISDIGIT(static_cast<unsigned>(*f))) {
        if (precision > (kMaxWord - (static_cast<int>(*f) - '0')) / 10) {
          Thread::current()->raiseWithFmt(LayoutId::kValueError,
                                          "precision too big");
          return nullptr;
        }
        precision = (precision * 10) + (*f - '0');
        f++;
      }
    }
    if (*f == '%') {
      // "%.3%s" => f points to "3"
      f--;
    }
  }
  if (*f == '\0') {
    // bogus format "%.123" => go backward, f points to "3"
    f--;
  }

  // Handle %ld, %lu, %lld and %llu.
  int longflag = 0;
  int longlongflag = 0;
  int size_tflag = 0;
  if (*f == 'l') {
    if (f[1] == 'd' || f[1] == 'u' || f[1] == 'i') {
      longflag = 1;
      ++f;
    } else if (f[1] == 'l' && (f[2] == 'd' || f[2] == 'u' || f[2] == 'i')) {
      longlongflag = 1;
      f += 2;
    }
  }
  // handle the size_t flag.
  else if (*f == 'z' && (f[1] == 'd' || f[1] == 'u' || f[1] == 'i')) {
    size_tflag = 1;
    ++f;
  }

  if (f[1] == '\0') writer->overallocate = 0;

  switch (*f) {
    case 'c': {
      int ordinal = va_arg(*vargs, int);
      if (ordinal < 0 || ordinal > kMaxUnicode) {
        Thread::current()->raiseWithFmt(
            LayoutId::kOverflowError,
            "character argument not in range(0x110000)");
        return nullptr;
      }
      if (_PyUnicodeWriter_WriteCharInline(writer, ordinal) < 0) return nullptr;
      break;
    }

    case 'i':
    case 'd':
    case 'u':
    case 'x': {
      // used by sprintf
      char buffer[kMaxLongLongChars];
      Py_ssize_t len;

      if (*f == 'u') {
        if (longflag) {
          len = std::sprintf(buffer, "%lu", va_arg(*vargs, unsigned long));
        } else if (longlongflag) {
          len =
              std::sprintf(buffer, "%llu", va_arg(*vargs, unsigned long long));
        } else if (size_tflag) {
          len = std::sprintf(buffer, "%" PY_FORMAT_SIZE_T "u",
                             va_arg(*vargs, size_t));
        } else {
          len = std::sprintf(buffer, "%u", va_arg(*vargs, unsigned int));
        }
      } else if (*f == 'x') {
        len = std::sprintf(buffer, "%x", va_arg(*vargs, int));
      } else {
        if (longflag) {
          len = std::sprintf(buffer, "%li", va_arg(*vargs, long));
        } else if (longlongflag) {
          len = std::sprintf(buffer, "%lli", va_arg(*vargs, long long));
        } else if (size_tflag) {
          len = std::sprintf(buffer, "%" PY_FORMAT_SIZE_T "i",
                             va_arg(*vargs, Py_ssize_t));
        } else {
          len = std::sprintf(buffer, "%i", va_arg(*vargs, int));
        }
      }
      DCHECK(len >= 0, "len must be >= 0");

      if (precision < len) precision = len;

      Py_ssize_t arglen = Py_MAX(precision, width);
      if (_PyUnicodeWriter_Prepare(writer, arglen, 127) == -1) return nullptr;

      if (width > precision) {
        Py_ssize_t fill = width - precision;
        Py_UCS4 fillchar = zeropad ? '0' : ' ';
        // Facebook: Our internal strings are immutable, can't use
        // PyUnicode_Fill (D13491655)
        for (Py_ssize_t i = 0; i < fill; ++i) {
          if (_PyUnicodeWriter_WriteCharInline(writer, fillchar) == -1) {
            return nullptr;
          }
        }
      }
      if (precision > len) {
        Py_ssize_t fill = precision - len;
        // Facebook: Our internal strings are immutable, can't use
        // PyUnicode_Fill (D13491655)
        for (Py_ssize_t i = 0; i < fill; ++i) {
          if (_PyUnicodeWriter_WriteCharInline(writer, '0') == -1) {
            return nullptr;
          }
        }
      }

      if (_PyUnicodeWriter_WriteASCIIString(writer, buffer, len) < 0) {
        return nullptr;
      }
      break;
    }

    case 'p': {
      char number[kMaxLongLongChars];

      Py_ssize_t len = std::sprintf(number, "%p", va_arg(*vargs, void*));
      DCHECK(len >= 0, "len must be >= 0");

      // %p is ill-defined:  ensure leading 0x.
      if (number[1] == 'X') {
        number[1] = 'x';
      } else if (number[1] != 'x') {
        std::memmove(number + 2, number, std::strlen(number) + 1);
        number[0] = '0';
        number[1] = 'x';
        len += 2;
      }

      if (_PyUnicodeWriter_WriteASCIIString(writer, number, len) < 0) {
        return nullptr;
      }
      break;
    }

    case 's': {
      // UTF-8
      const char* s = va_arg(*vargs, const char*);
      if (writeCStr(writer, s, width, precision) < 0) {
        return nullptr;
      }
      break;
    }

    case 'U': {
      PyObject* obj = va_arg(*vargs, PyObject*);
      // This used to call _PyUnicode_CHECK, which is deprecated, and which we
      // have not imported.
      DCHECK(obj, "obj must not be null");

      if (writeStr(writer, obj, width, precision) == -1) {
        return nullptr;
      }
      break;
    }

    case 'V': {
      PyObject* obj = va_arg(*vargs, PyObject*);
      const char* str = va_arg(*vargs, const char*);
      if (obj) {
        // This used to DCHECK _PyUnicode_CHECK, which is deprecated, and which
        // we have not imported.
        if (writeStr(writer, obj, width, precision) == -1) {
          return nullptr;
        }
      } else {
        DCHECK(str != nullptr, "str must not be null");
        if (writeCStr(writer, str, width, precision) < 0) {
          return nullptr;
        }
      }
      break;
    }

    case 'S': {
      PyObject* obj = va_arg(*vargs, PyObject*);
      DCHECK(obj, "obj must not be null");
      PyObject* str = PyObject_Str(obj);
      if (!str) return nullptr;
      if (writeStr(writer, str, width, precision) == -1) {
        Py_DECREF(str);
        return nullptr;
      }
      Py_DECREF(str);
      break;
    }

    case 'R': {
      PyObject* obj = va_arg(*vargs, PyObject*);
      DCHECK(obj, "obj must not be null");
      PyObject* repr = PyObject_Repr(obj);
      if (!repr) return nullptr;
      if (writeStr(writer, repr, width, precision) == -1) {
        Py_DECREF(repr);
        return nullptr;
      }
      Py_DECREF(repr);
      break;
    }

    case 'A': {
      PyObject* obj = va_arg(*vargs, PyObject*);
      DCHECK(obj, "obj must not be null");
      PyObject* ascii = PyObject_ASCII(obj);
      if (!ascii) return nullptr;
      if (writeStr(writer, ascii, width, precision) == -1) {
        Py_DECREF(ascii);
        return nullptr;
      }
      Py_DECREF(ascii);
      break;
    }

    case '%':
      if (_PyUnicodeWriter_WriteCharInline(writer, '%') < 0) return nullptr;
      break;

    default: {
      // if we stumble upon an unknown formatting code, copy the rest
      // of the format string to the output string. (we cannot just
      // skip the code, since there's no way to know what's in the
      // argument list)
      Py_ssize_t len = std::strlen(p);
      if (_PyUnicodeWriter_WriteLatin1String(writer, p, len) == -1) {
        return nullptr;
      }
      f = p + len;
      return f;
    }
  }

  f++;
  return f;
}

PY_EXPORT int _PyUnicode_EqualToASCIIString(PyObject* unicode,
                                            const char* c_str) {
  DCHECK(unicode, "nullptr argument");
  DCHECK(c_str, "nullptr argument");
  RawObject obj = ApiHandle::asObject(ApiHandle::fromPyObject(unicode));
  DCHECK(Thread::current()->runtime()->isInstanceOfStr(obj),
         "non-str argument");
  return strUnderlying(obj).equalsCStr(c_str);
}

PY_EXPORT int _PyUnicode_EQ(PyObject* aa, PyObject* bb) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj_aa(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(aa)));
  Object obj_bb(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(bb)));
  Str lhs(&scope, strUnderlying(*obj_aa));
  Str rhs(&scope, strUnderlying(*obj_bb));
  return lhs.equals(*rhs);
}

PY_EXPORT size_t Py_UNICODE_strlen(const Py_UNICODE* u) {
  DCHECK(u != nullptr, "u should not be null");
  return std::wcslen(u);
}

PY_EXPORT int _PyUnicode_Ready(PyObject* /* unicode */) { return 0; }

PY_EXPORT int PyUnicode_CheckExact_Func(PyObject* obj) {
  return ApiHandle::asObject(ApiHandle::fromPyObject(obj)).isStr();
}

PY_EXPORT int PyUnicode_Check_Func(PyObject* obj) {
  return Thread::current()->runtime()->isInstanceOfStr(
      ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
}

PY_EXPORT PyObject* PyUnicode_FromString(const char* c_string) {
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReference(runtime, runtime->newStrFromCStr(c_string));
}

// Look for a surrogate codepoint in str[start:]. Note that start is a byte
// offset. Return the first index found in that range, or -1 if not found.
static word strFindSurrogateCodepoint(const Str& str, word start) {
  word length = str.length();
  word byte_index = start;
  while (byte_index < length) {
    word num_bytes;
    int32_t codepoint = str.codePointAt(byte_index, &num_bytes);
    if (Unicode::isSurrogate(codepoint)) {
      return byte_index;
    }
    byte_index += num_bytes;
  }
  return -1;
}

PY_EXPORT const char* PyUnicode_AsUTF8AndSize(PyObject* pyunicode,
                                              Py_ssize_t* size) {
  Thread* thread = Thread::current();
  if (pyunicode == nullptr) {
    thread->raiseBadArgument();
    return nullptr;
  }

  HandleScope scope(thread);
  ApiHandle* handle = ApiHandle::fromPyObject(pyunicode);
  Object obj(&scope, ApiHandle::asObject(handle));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*obj)) {
    thread->raiseBadInternalCall();
    return nullptr;
  }

  Str str(&scope, strUnderlying(*obj));
  word length = str.length();
  if (size != nullptr) *size = length;
  if (void* cache = ApiHandle::cache(runtime, handle)) {
    return static_cast<char*>(cache);
  }

  word surr_index = strFindSurrogateCodepoint(str, 0);
  if (surr_index != -1) {
    Object encoding(&scope, SmallStr::fromCStr("utf-8"));
    Object start(&scope, SmallInt::fromWord(surr_index));
    Object end(&scope, SmallInt::fromWord(surr_index + 1));
    Object reason(&scope, runtime->newStrFromCStr("surrogates not allowed"));
    Object exc(&scope,
               thread->invokeFunction5(ID(builtins), ID(UnicodeEncodeError),
                                       encoding, str, start, end, reason));
    Object err(&scope,
               thread->invokeFunction1(ID(_codecs), ID(strict_errors), exc));
    DCHECK(err.isErrorException(),
           "_codecs.strict_errors should raise an exception");
    return nullptr;
  }

  byte* result = static_cast<byte*>(std::malloc(length + 1));
  str.copyTo(result, length);
  result[length] = '\0';
  ApiHandle::setCache(runtime, handle, result);
  ApiHandle::setBorrowedNoImmediate(handle);
  return reinterpret_cast<char*>(result);
}

PY_EXPORT const char* PyUnicode_AsUTF8(PyObject* unicode) {
  return PyUnicode_AsUTF8AndSize(unicode, nullptr);
}

PY_EXPORT PyObject* PyUnicode_FromStringAndSize(const char* u,
                                                Py_ssize_t size) {
  Thread* thread = Thread::current();

  if (size < 0) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "Negative size passed to PyUnicode_FromStringAndSize");
    return nullptr;
  }
  if (u == nullptr && size != 0) {
    // TODO(T36562134): Implement _PyUnicode_New
    UNIMPLEMENTED("_PyUnicode_New");
  }
  const byte* data = reinterpret_cast<const byte*>(u);
  Runtime* runtime = thread->runtime();
  return ApiHandle::newReference(
      runtime, runtime->newStrWithAll(View<byte>(data, size)));
}

PY_EXPORT PyObject* PyUnicode_EncodeFSDefault(PyObject* unicode) {
  // TODO(T40363016): Allow arbitrary encodings instead of defaulting to utf-8
  return _PyUnicode_AsUTF8String(unicode, Py_FileSystemDefaultEncodeErrors);
}

PY_EXPORT PyObject* PyUnicode_New(Py_ssize_t size, Py_UCS4 maxchar) {
  Thread* thread = Thread::current();
  // Since CPython optimizes for empty string, we must do so as well to make
  // sure we don't fail if maxchar is invalid
  if (size == 0) {
    return ApiHandle::newReference(thread->runtime(), Str::empty());
  }
  if (maxchar > kMaxUnicode) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "invalid maximum character passed to PyUnicode_New");
    return nullptr;
  }
  if (size < 0) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "Negative size passed to PyUnicode_New");
    return nullptr;
  }
  // TODO(T41498010): Add modifiable string state
  UNIMPLEMENTED("Cannot create mutable strings yet");
}

PY_EXPORT void PyUnicode_Append(PyObject** p_left, PyObject* right) {
  if (p_left == nullptr) {
    if (!PyErr_Occurred()) {
      PyErr_BadInternalCall();
    }
    return;
  }

  PyObject* left = *p_left;
  if (left == nullptr || right == nullptr || !PyUnicode_Check(left) ||
      !PyUnicode_Check(right)) {
    if (!PyErr_Occurred()) {
      PyErr_BadInternalCall();
    }
    Py_CLEAR(*p_left);
    return;
  }
  *p_left = PyUnicode_Concat(left, right);
  Py_DECREF(left);
}

PY_EXPORT void PyUnicode_AppendAndDel(PyObject** p_left, PyObject* right) {
  PyUnicode_Append(p_left, right);
  Py_XDECREF(right);
}

PY_EXPORT PyObject* _PyUnicode_AsASCIIString(PyObject* unicode,
                                             const char* errors) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*str)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object tuple_obj(&scope, thread->invokeFunction2(
                               ID(_codecs), ID(ascii_encode), str, errors_obj));
  if (tuple_obj.isError()) {
    return nullptr;
  }
  Tuple tuple(&scope, *tuple_obj);
  return ApiHandle::newReference(runtime, tuple.at(0));
}

PY_EXPORT PyObject* PyUnicode_AsASCIIString(PyObject* unicode) {
  return _PyUnicode_AsASCIIString(unicode, "strict");
}

PY_EXPORT PyObject* PyUnicode_AsCharmapString(PyObject* /* e */,
                                              PyObject* /* g */) {
  UNIMPLEMENTED("PyUnicode_AsCharmapString");
}

PY_EXPORT PyObject* PyUnicode_AsDecodedObject(PyObject* /* e */,
                                              const char* /* g */,
                                              const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_AsDecodedObject");
}

PY_EXPORT PyObject* PyUnicode_AsDecodedUnicode(PyObject* /* e */,
                                               const char* /* g */,
                                               const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_AsDecodedUnicode");
}

PY_EXPORT PyObject* PyUnicode_AsEncodedObject(PyObject* /* e */,
                                              const char* /* g */,
                                              const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_AsEncodedObject");
}

PY_EXPORT PyObject* PyUnicode_AsEncodedString(PyObject* unicode,
                                              const char* encoding,
                                              const char* errors) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  if (encoding == nullptr) {
    return _PyUnicode_AsUTF8String(unicode, errors);
  }
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*str)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Object encoding_obj(&scope, runtime->newStrFromCStr(encoding));
  Object errors_obj(&scope, errors == nullptr
                                ? Unbound::object()
                                : symbolFromError(thread, errors));
  Object result(&scope, thread->invokeFunction3(ID(_codecs), ID(encode), str,
                                                encoding_obj, errors_obj));
  if (result.isError()) {
    return nullptr;
  }
  if (runtime->isInstanceOfBytes(*result)) {
    return ApiHandle::newReference(runtime, *result);
  }
  if (runtime->isInstanceOfBytearray(*result)) {
    // Equivalent to calling PyErr_WarnFormat
    if (!ensureBuiltinModuleById(thread, ID(warnings)).isErrorException()) {
      Object category(&scope, runtime->typeAt(LayoutId::kRuntimeWarning));
      Object message(&scope,
                     runtime->newStrFromFmt(
                         "encoder %s returned bytearray instead of bytes; "
                         "use codecs.encode() to encode to arbitrary types",
                         encoding));
      Object stack_level(&scope, runtime->newInt(1));
      Object source(&scope, NoneType::object());
      Object err(&scope,
                 thread->invokeFunction4(ID(warnings), ID(warn), message,
                                         category, stack_level, source));
      if (err.isErrorException()) {
        thread->clearPendingException();
      }
    }
    Bytearray result_bytearray(&scope, *result);
    return ApiHandle::newReference(runtime,
                                   bytearrayAsBytes(thread, result_bytearray));
  }
  thread->raiseWithFmt(LayoutId::kTypeError,
                       "'%s' encoder returned '%T' instead of 'bytes'; "
                       "use codecs.encode() to encode to arbitrary types",
                       encoding, *result);
  return nullptr;
}

PY_EXPORT PyObject* PyUnicode_AsEncodedUnicode(PyObject* /* e */,
                                               const char* /* g */,
                                               const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_AsEncodedUnicode");
}

PY_EXPORT PyObject* _PyUnicode_AsLatin1String(PyObject* unicode,
                                              const char* errors) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*str)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object tuple_obj(&scope,
                   thread->invokeFunction2(ID(_codecs), ID(latin_1_encode), str,
                                           errors_obj));
  if (tuple_obj.isError()) {
    return nullptr;
  }
  Tuple tuple(&scope, *tuple_obj);
  return ApiHandle::newReference(runtime, tuple.at(0));
}

PY_EXPORT PyObject* PyUnicode_AsLatin1String(PyObject* unicode) {
  return _PyUnicode_AsLatin1String(unicode, "strict");
}

PY_EXPORT PyObject* PyUnicode_AsMBCSString(PyObject* /* e */) {
  UNIMPLEMENTED("PyUnicode_AsMBCSString");
}

PY_EXPORT PyObject* PyUnicode_AsRawUnicodeEscapeString(PyObject* /* e */) {
  UNIMPLEMENTED("PyUnicode_AsRawUnicodeEscapeString");
}

PY_EXPORT Py_UCS4* PyUnicode_AsUCS4(PyObject* u, Py_UCS4* buffer,
                                    Py_ssize_t buflen, int copy_null) {
  if (buffer == nullptr || buflen < 0) {
    PyErr_BadInternalCall();
    return nullptr;
  }

  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(u)));
  if (!thread->runtime()->isInstanceOfStr(*obj)) {
    thread->raiseBadArgument();
  }

  Str str(&scope, strUnderlying(*obj));
  word num_codepoints = str.codePointLength();
  word target_buflen = copy_null ? num_codepoints + 1 : num_codepoints;
  if (buflen < target_buflen) {
    thread->raiseWithFmt(LayoutId::kSystemError,
                         "string is longer than the buffer");
    if (copy_null != 0 && 0 < buflen) {
      buffer[0] = 0;
    }
    return nullptr;
  }

  for (word i = 0, offset = 0; i < num_codepoints; i++) {
    word num_bytes;
    buffer[i] = str.codePointAt(offset, &num_bytes);
    offset += num_bytes;
  }
  if (copy_null != 0) buffer[num_codepoints] = 0;

  return buffer;
}

PY_EXPORT Py_UCS4* PyUnicode_AsUCS4Copy(PyObject* str) {
  Py_ssize_t len = PyUnicode_GET_LENGTH(str) + 1;
  Py_UCS4* result = static_cast<Py_UCS4*>(PyMem_Malloc(len * sizeof(Py_UCS4)));
  if (result == nullptr) {
    PyErr_NoMemory();
    return nullptr;
  }
  return PyUnicode_AsUCS4(str, result, len, 1);
}

PY_EXPORT PyObject* PyUnicode_AsUTF16String(PyObject* unicode) {
  return _PyUnicode_EncodeUTF16(unicode, nullptr, 0);
}

PY_EXPORT PyObject* PyUnicode_AsUTF32String(PyObject* unicode) {
  return _PyUnicode_EncodeUTF32(unicode, nullptr, 0);
}

PY_EXPORT PyObject* PyUnicode_AsUTF8String(PyObject* unicode) {
  return _PyUnicode_AsUTF8String(unicode, "strict");
}

PY_EXPORT PyObject* PyUnicode_AsUnicodeEscapeString(PyObject* /* e */) {
  UNIMPLEMENTED("PyUnicode_AsUnicodeEscapeString");
}

PY_EXPORT Py_ssize_t PyUnicode_AsWideChar(PyObject* str, wchar_t* result,
                                          Py_ssize_t size) {
  Thread* thread = Thread::current();
  if (str == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*str_obj)) {
    thread->raiseWithFmt(
        LayoutId::kTypeError,
        "PyUnicode_AsWideChar requires 'str' object but received a '%T'",
        &str_obj);
    return -1;
  }
  Str str_str(&scope, strUnderlying(*str_obj));
  Py_ssize_t num_code_points = str_str.codePointLength();
  if (size > num_code_points) {
    size = num_code_points + 1;
  } else {
    num_code_points = size;
  }

  {
    word byte_count = str_str.length();
    for (word byte_index = 0, wchar_index = 0, num_bytes = 0;
         byte_index < byte_count && wchar_index < size;
         byte_index += num_bytes, wchar_index += 1) {
      int32_t cp = str_str.codePointAt(byte_index, &num_bytes);
      static_assert(sizeof(wchar_t) == sizeof(cp), "Requires 32bit wchar_t");
      if (result != nullptr) {
        result[wchar_index] = static_cast<wchar_t>(cp);
      }
    }
    if (num_code_points < size) {
      result[num_code_points] = '\0';
    }
  }

  return num_code_points;
}

PY_EXPORT wchar_t* PyUnicode_AsWideCharString(PyObject* str,
                                              Py_ssize_t* result_len) {
  Thread* thread = Thread::current();
  if (str == nullptr) {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*str_obj)) {
    thread->raiseWithFmt(
        LayoutId::kTypeError,
        "PyUnicode_AsWideChar requires 'str' object but received a '%T'",
        &str_obj);
    return nullptr;
  }
  Str str_str(&scope, strUnderlying(*str_obj));
  word length = str_str.codePointLength();
  wchar_t* result =
      static_cast<wchar_t*>(PyMem_Malloc((length + 1) * sizeof(wchar_t)));
  if (result == nullptr) {
    thread->raiseMemoryError();
    return nullptr;
  }

  {
    word byte_count = str_str.length();
    for (word byte_index = 0, wchar_index = 0, num_bytes = 0;
         byte_index < byte_count && wchar_index < length + 1;
         byte_index += num_bytes, wchar_index += 1) {
      int32_t cp = str_str.codePointAt(byte_index, &num_bytes);
      if (cp == '\0') {
        PyMem_Free(result);
        thread->raiseWithFmt(LayoutId::kValueError, "embedded null character");
        return nullptr;
      }
      static_assert(sizeof(wchar_t) == sizeof(cp), "Requires 32bit wchar_t");
      result[wchar_index] = static_cast<wchar_t>(cp);
    }
    result[length] = '\0';
  }

  if (result_len != nullptr) {
    *result_len = length;
  }
  return result;
}

PY_EXPORT PyObject* PyUnicode_BuildEncodingMap(PyObject* /* g */) {
  UNIMPLEMENTED("PyUnicode_BuildEncodingMap");
}

PY_EXPORT int PyUnicode_Compare(PyObject* left, PyObject* right) {
  Thread* thread = Thread::current();
  if (left == nullptr || right == nullptr) {
    thread->raiseBadInternalCall();
    return -1;
  }

  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object left_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(left)));
  Object right_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(right)));
  if (runtime->isInstanceOfStr(*left_obj) &&
      runtime->isInstanceOfStr(*right_obj)) {
    Str left_str(&scope, strUnderlying(*left_obj));
    Str right_str(&scope, strUnderlying(*right_obj));
    word result = left_str.compare(*right_str);
    return result > 0 ? 1 : (result < 0 ? -1 : 0);
  }
  thread->raiseWithFmt(LayoutId::kTypeError, "Can't compare %T and %T",
                       &left_obj, &right_obj);
  return -1;
}

PY_EXPORT int PyUnicode_CompareWithASCIIString(PyObject* uni, const char* str) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(uni)));
  Str str_obj(&scope, strUnderlying(*obj));
  // TODO(atalaba): Allow for proper comparison against Latin-1 strings. For
  // example, in CPython: "\xC3\xA9" (UTF-8) == "\xE9" (Latin-1), and
  // "\xE9 longer" > "\xC3\xA9".
  return str_obj.compareCStr(str);
}

PY_EXPORT PyObject* PyUnicode_Concat(PyObject* left, PyObject* right) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();

  Object left_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(left)));
  Object right_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(right)));
  if (!runtime->isInstanceOfStr(*left_obj) ||
      !runtime->isInstanceOfStr(*right_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError,
                         "can only concatenate str to str");
    return nullptr;
  }
  Str left_str(&scope, strUnderlying(*left_obj));
  Str right_str(&scope, strUnderlying(*right_obj));
  word dummy;
  if (__builtin_add_overflow(left_str.length(), right_str.length(), &dummy)) {
    thread->raiseWithFmt(LayoutId::kOverflowError,
                         "strings are too large to concat");
    return nullptr;
  }
  return ApiHandle::newReference(
      runtime, runtime->strConcat(thread, left_str, right_str));
}

PY_EXPORT int PyUnicode_Contains(PyObject* str, PyObject* substr) {
  DCHECK(str != nullptr, "str should not be null");
  DCHECK(substr != nullptr, "substr should not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object substr_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(substr)));
  Object result(&scope,
                thread->invokeMethodStatic2(LayoutId::kStr, ID(__contains__),
                                            str_obj, substr_obj));
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError,
                           "could not call str.__contains__");
    }
    return -1;
  }
  DCHECK(result.isBool(), "result of __contains__ should be bool");
  return Bool::cast(*result).value();
}

PY_EXPORT Py_ssize_t PyUnicode_CopyCharacters(PyObject*, Py_ssize_t, PyObject*,
                                              Py_ssize_t, Py_ssize_t) {
  UNIMPLEMENTED("PyUnicode_CopyCharacters");
}

PY_EXPORT Py_ssize_t PyUnicode_Count(PyObject* /* r */, PyObject* /* r */,
                                     Py_ssize_t /* t */, Py_ssize_t /* d */) {
  UNIMPLEMENTED("PyUnicode_Count");
}

PY_EXPORT PyObject* PyUnicode_Decode(const char* c_str, Py_ssize_t size,
                                     const char* encoding, const char* errors) {
  DCHECK(c_str != nullptr, "c_str cannot be null");
  if (encoding == nullptr) {
    return PyUnicode_DecodeUTF8Stateful(c_str, size, errors, nullptr);
  }

  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Bytes bytes(&scope, runtime->newBytesWithAll(View<byte>(
                          reinterpret_cast<const byte*>(c_str), size)));
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object encoding_obj(&scope, runtime->newStrFromCStr(encoding));
  Object result(&scope, thread->invokeFunction3(ID(_codecs), ID(decode), bytes,
                                                encoding_obj, errors_obj));
  if (result.isError()) {
    return nullptr;
  }
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT PyObject* PyUnicode_DecodeASCII(const char* c_str, Py_ssize_t size,
                                          const char* errors) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Bytes bytes(&scope, runtime->newBytesWithAll(View<byte>(
                          reinterpret_cast<const byte*>(c_str), size)));
  Str errors_obj(&scope, symbolFromError(thread, errors));
  Object result_obj(
      &scope, thread->invokeFunction2(ID(_codecs), ID(ascii_decode), bytes,
                                      errors_obj));
  if (result_obj.isError()) {
    if (result_obj.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kSystemError,
                           "could not call _codecs.ascii_decode");
    }
    return nullptr;
  }
  Tuple result(&scope, *result_obj);
  return ApiHandle::newReference(runtime, result.at(0));
}

PY_EXPORT PyObject* PyUnicode_DecodeCharmap(const char* /* s */,
                                            Py_ssize_t /* e */,
                                            PyObject* /* g */,
                                            const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_DecodeCharmap");
}

PY_EXPORT PyObject* PyUnicode_DecodeCodePageStateful(int /* e */,
                                                     const char* /* s */,
                                                     Py_ssize_t /* e */,
                                                     const char* /* s */,
                                                     Py_ssize_t* /* d */) {
  UNIMPLEMENTED("PyUnicode_DecodeCodePageStateful");
}

PY_EXPORT PyObject* PyUnicode_DecodeFSDefault(const char* c_str) {
  Runtime* runtime = Thread::current()->runtime();
  return ApiHandle::newReference(runtime, runtime->newStrFromCStr(c_str));
}

PY_EXPORT PyObject* PyUnicode_DecodeFSDefaultAndSize(const char* c_str,
                                                     Py_ssize_t size) {
  Runtime* runtime = Thread::current()->runtime();
  View<byte> str(reinterpret_cast<const byte*>(c_str), size);
  return ApiHandle::newReference(runtime, runtime->newStrWithAll(str));
}

PY_EXPORT PyObject* PyUnicode_DecodeLatin1(const char* c_str, Py_ssize_t size,
                                           const char* /* errors */) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Bytes bytes(&scope, runtime->newBytesWithAll(View<byte>(
                          reinterpret_cast<const byte*>(c_str), size)));
  Object result_obj(
      &scope, thread->invokeFunction1(ID(_codecs), ID(latin_1_decode), bytes));
  if (result_obj.isError()) {
    if (result_obj.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kSystemError,
                           "could not call _codecs.latin_1_decode");
    }
    return nullptr;
  }
  Tuple result(&scope, *result_obj);
  return ApiHandle::newReference(runtime, result.at(0));
}

PY_EXPORT PyObject* PyUnicode_DecodeLocale(const char* str,
                                           const char* errors) {
  return PyUnicode_DecodeLocaleAndSize(str, std::strlen(str), errors);
}

PY_EXPORT PyObject* PyUnicode_DecodeLocaleAndSize(const char* str,
                                                  Py_ssize_t len,
                                                  const char* errors) {
  _Py_error_handler surrogateescape;
  if (errors == nullptr || std::strcmp(errors, "strict") == 0) {
    surrogateescape = _Py_ERROR_STRICT;
  } else if (std::strcmp(errors, "surrogateescape") == 0) {
    surrogateescape = _Py_ERROR_SURROGATEESCAPE;
  } else {
    Thread::current()->raiseWithFmt(
        LayoutId::kValueError,
        "only 'strict' and 'surrogateescape' error handlers "
        "are supported, not '%s'",
        errors);
    return nullptr;
  }

  if (str[len] != '\0' || static_cast<size_t>(len) != std::strlen(str)) {
    Thread::current()->raiseWithFmt(LayoutId::kValueError,
                                    "embedded null byte");
    return nullptr;
  }

  wchar_t* wstr;
  size_t wlen;
  const char* reason;
  int res = _Py_DecodeLocaleEx(str, &wstr, &wlen, &reason, 1, surrogateescape);
  if (res != 0) {
    if (res == -2) {
      PyObject* exc =
          PyObject_CallFunction(PyExc_UnicodeDecodeError, "sy#nns", "locale",
                                str, len, wlen, wlen + 1, reason);
      if (exc != nullptr) {
        PyCodec_StrictErrors(exc);
        Py_DECREF(exc);
      }
    } else {
      PyErr_NoMemory();
    }
    return nullptr;
  }

  PyObject* unicode = PyUnicode_FromWideChar(wstr, wlen);
  PyMem_RawFree(wstr);
  return unicode;
}

PY_EXPORT PyObject* PyUnicode_DecodeMBCS(const char* /* s */,
                                         Py_ssize_t /* e */,
                                         const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_DecodeMBCS");
}

PY_EXPORT PyObject* PyUnicode_DecodeMBCSStateful(const char* /* s */,
                                                 Py_ssize_t /* e */,
                                                 const char* /* s */,
                                                 Py_ssize_t* /* d */) {
  UNIMPLEMENTED("PyUnicode_DecodeMBCSStateful");
}

PY_EXPORT PyObject* PyUnicode_DecodeRawUnicodeEscape(const char* /* s */,
                                                     Py_ssize_t /* e */,
                                                     const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_DecodeRawUnicodeEscape");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF16(const char* /* s */,
                                          Py_ssize_t /* e */,
                                          const char* /* s */, int* /* r */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF16");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF16Stateful(const char* /* s */,
                                                  Py_ssize_t /* e */,
                                                  const char* /* s */,
                                                  int* /* r */,
                                                  Py_ssize_t* /* d */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF16Stateful");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF32(const char* /* s */,
                                          Py_ssize_t /* e */,
                                          const char* /* s */, int* /* r */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF32");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF32Stateful(const char* /* s */,
                                                  Py_ssize_t /* e */,
                                                  const char* /* s */,
                                                  int* /* r */,
                                                  Py_ssize_t* /* d */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF32Stateful");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF7(const char* /* s */,
                                         Py_ssize_t /* e */,
                                         const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF7");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF7Stateful(const char* /* s */,
                                                 Py_ssize_t /* e */,
                                                 const char* /* s */,
                                                 Py_ssize_t* /* d */) {
  UNIMPLEMENTED("PyUnicode_DecodeUTF7Stateful");
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF8(const char* c_str, Py_ssize_t size,
                                         const char* errors) {
  return PyUnicode_DecodeUTF8Stateful(c_str, size, errors, nullptr);
}

PY_EXPORT PyObject* PyUnicode_DecodeUTF8Stateful(const char* c_str,
                                                 Py_ssize_t size,
                                                 const char* errors,
                                                 Py_ssize_t* consumed) {
  DCHECK(c_str != nullptr, "c_str cannot be null");

  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  word i = 0;
  const byte* byte_str = reinterpret_cast<const byte*>(c_str);
  for (; i < size; ++i) {
    if (byte_str[i] > kMaxASCII) break;
  }
  if (i == size) {
    if (consumed != nullptr) {
      *consumed = size;
    }
    return ApiHandle::newReference(runtime,
                                   runtime->newStrWithAll({byte_str, size}));
  }
  Object bytes(&scope, runtime->newBytesWithAll(View<byte>({byte_str, size})));
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object is_final(&scope, Bool::fromBool(consumed == nullptr));
  Object result_obj(
      &scope, thread->invokeFunction3(ID(_codecs), ID(utf_8_decode), bytes,
                                      errors_obj, is_final));
  if (result_obj.isError()) {
    if (result_obj.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kSystemError,
                           "could not call _codecs._utf_8_decode_stateful");
    }
    return nullptr;
  }
  Tuple result(&scope, *result_obj);
  if (consumed != nullptr) {
    *consumed = Int::cast(result.at(1)).asWord();
  }
  return ApiHandle::newReference(runtime, result.at(0));
}

PY_EXPORT PyObject* PyUnicode_DecodeUnicodeEscape(const char* c_str,
                                                  Py_ssize_t size,
                                                  const char* errors) {
  DCHECK(c_str != nullptr, "c_str cannot be null");
  const char* first_invalid_escape;
  PyObject* result = _PyUnicode_DecodeUnicodeEscape(c_str, size, errors,
                                                    &first_invalid_escape);
  if (result == nullptr) {
    return nullptr;
  }
  if (first_invalid_escape != nullptr) {
    if (PyErr_WarnFormat(PyExc_DeprecationWarning, 1,
                         "invalid escape sequence '\\%c'",
                         static_cast<byte>(*first_invalid_escape)) < 0) {
      Py_DECREF(result);
      return nullptr;
    }
  }
  return result;
}

PY_EXPORT PyObject* _PyUnicode_DecodeUnicodeEscape(
    const char* c_str, Py_ssize_t size, const char* errors,
    const char** first_invalid_escape) {
  DCHECK(c_str != nullptr, "c_str cannot be null");
  DCHECK(first_invalid_escape != nullptr,
         "first_invalid_escape cannot be null");

  // So we can remember if we've seen an invalid escape char or not
  *first_invalid_escape = nullptr;

  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object bytes(&scope, runtime->newBytesWithAll(View<byte>(
                           reinterpret_cast<const byte*>(c_str), size)));
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object result_obj(
      &scope,
      thread->invokeFunction2(ID(_codecs), ID(_unicode_escape_decode_stateful),
                              bytes, errors_obj));
  if (result_obj.isError()) {
    if (result_obj.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kSystemError,
                           "could not call _codecs.unicode_escape_decode");
    }
    return nullptr;
  }
  Tuple result(&scope, *result_obj);
  Int first_invalid_index(&scope, result.at(2));
  word invalid_index = first_invalid_index.asWord();
  if (invalid_index > -1) {
    *first_invalid_escape = c_str + invalid_index;
  }
  return ApiHandle::newReference(runtime, result.at(0));
}

PY_EXPORT PyObject* PyUnicode_EncodeCodePage(int /* e */, PyObject* /* e */,
                                             const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_EncodeCodePage");
}

PY_EXPORT PyObject* PyUnicode_EncodeLocale(PyObject* unicode,
                                           const char* errors) {
  _Py_error_handler surrogateescape;
  if (errors == nullptr || std::strcmp(errors, "strict") == 0) {
    surrogateescape = _Py_ERROR_STRICT;
  } else if (std::strcmp(errors, "surrogateescape") == 0) {
    surrogateescape = _Py_ERROR_SURROGATEESCAPE;
  } else {
    Thread::current()->raiseWithFmt(
        LayoutId::kValueError,
        "only 'strict' and 'surrogateescape' error handlers "
        "are supported, not '%s'",
        errors);
    return nullptr;
  }
  Py_ssize_t wlen;
  wchar_t* wstr = PyUnicode_AsWideCharString(unicode, &wlen);
  if (wstr == nullptr) {
    return nullptr;
  }

  if (static_cast<size_t>(wlen) != std::wcslen(wstr)) {
    Thread::current()->raiseWithFmt(LayoutId::kValueError,
                                    "embedded null character");
    PyMem_Free(wstr);
    return nullptr;
  }

  char* str;
  size_t error_pos;
  const char* reason;
  int res = _Py_EncodeLocaleEx(wstr, &str, &error_pos, &reason,
                               /*current_locale=*/1, surrogateescape);
  PyMem_Free(wstr);

  if (res != 0) {
    if (res == -2) {
      PyObject* exc =
          PyObject_CallFunction(PyExc_UnicodeEncodeError, "sOnns", "locale",
                                unicode, error_pos, error_pos + 1, reason);
      if (exc != nullptr) {
        PyCodec_StrictErrors(exc);
        Py_DECREF(exc);
      }
    } else {
      PyErr_NoMemory();
    }
    return nullptr;
  }

  PyObject* bytes = PyBytes_FromString(str);
  PyMem_RawFree(str);
  return bytes;
}

PY_EXPORT PyObject* _PyUnicode_EncodeUTF16(PyObject* unicode,
                                           const char* errors, int byteorder) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*str)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object byteorder_obj(&scope, runtime->newInt(byteorder));
  Object tuple_obj(&scope,
                   thread->invokeFunction3(ID(_codecs), ID(utf_16_encode), str,
                                           errors_obj, byteorder_obj));
  if (tuple_obj.isError()) {
    return nullptr;
  }
  Tuple tuple(&scope, *tuple_obj);
  return ApiHandle::newReference(runtime, tuple.at(0));
}

PY_EXPORT PyObject* PyUnicode_EncodeUTF16(const Py_UNICODE* unicode,
                                          Py_ssize_t size, const char* errors,
                                          int byteorder) {
  PyObject* str = PyUnicode_FromUnicode(unicode, size);
  if (str == nullptr) return nullptr;
  PyObject* result = _PyUnicode_EncodeUTF16(str, errors, byteorder);
  Py_DECREF(str);
  return result;
}

PY_EXPORT PyObject* _PyUnicode_EncodeUTF32(PyObject* unicode,
                                           const char* errors, int byteorder) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*str)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object byteorder_obj(&scope, runtime->newInt(byteorder));
  Object tuple_obj(&scope,
                   thread->invokeFunction3(ID(_codecs), ID(utf_32_encode), str,
                                           errors_obj, byteorder_obj));
  if (tuple_obj.isError()) {
    return nullptr;
  }
  Tuple tuple(&scope, *tuple_obj);
  return ApiHandle::newReference(runtime, tuple.at(0));
}

PY_EXPORT PyObject* PyUnicode_EncodeUTF32(const Py_UNICODE* unicode,
                                          Py_ssize_t size, const char* errors,
                                          int byteorder) {
  PyObject* str = PyUnicode_FromUnicode(unicode, size);
  if (str == nullptr) return nullptr;
  PyObject* result = _PyUnicode_EncodeUTF32(str, errors, byteorder);
  Py_DECREF(str);
  return result;
}

PY_EXPORT int PyUnicode_FSConverter(PyObject* arg, void* addr) {
  if (arg == nullptr) {
    Py_DECREF(*reinterpret_cast<PyObject**>(addr));
    *reinterpret_cast<PyObject**>(addr) = nullptr;
    return 1;
  }
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object arg_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(arg)));
  Object path(&scope, NoneType::object());
  Runtime* runtime = thread->runtime();
  if (runtime->isInstanceOfStr(*arg_obj) ||
      runtime->isInstanceOfBytes(*arg_obj)) {
    path = *arg_obj;
  } else {
    path = thread->invokeFunction1(ID(_io), ID(_fspath), arg_obj);
    if (path.isErrorException()) {
      return 0;
    }
  }
  Object output(&scope, NoneType::object());
  if (runtime->isInstanceOfBytes(*path)) {
    output = *path;
  } else {
    CHECK(std::strcmp(Py_FileSystemDefaultEncoding, "utf-8") == 0, "");
    CHECK(std::strcmp(Py_FileSystemDefaultEncodeErrors, "surrogatepass") == 0,
          "");
    // PyOS_FSPath/_io._fspath guarantee their returned value is bytes or str.
    // This is an inlined PyUnicode_FSDecoder, which does a UTF-8 decode with
    // surrogatepass. Since our strings are UTF-8 with UTF-16 surrogates
    // (WTF-8), we can just copy the bytes out.
    Str path_str(&scope, strUnderlying(*path));
    word path_len = path_str.length();
    MutableBytes bytes(&scope, runtime->newMutableBytesUninitialized(path_len));
    bytes.replaceFromWithStr(0, *path_str, path_len);
    output = bytes.becomeImmutable();
  }
  Bytes underlying(&scope, bytesUnderlying(*output));
  if (underlying.findByte('\0', /*start=*/0, /*length=*/underlying.length()) !=
      -1) {
    thread->raiseWithFmt(LayoutId::kValueError, "embedded null byte");
    return 0;
  }
  *reinterpret_cast<PyObject**>(addr) =
      ApiHandle::newReference(runtime, *output);
  return Py_CLEANUP_SUPPORTED;
}

PY_EXPORT int PyUnicode_FSDecoder(PyObject* arg, void* addr) {
  if (arg == nullptr) {
    Py_DECREF(*(PyObject**)addr);
    *reinterpret_cast<PyObject**>(addr) = nullptr;
    return 1;
  }

  bool is_buffer = PyObject_CheckBuffer(arg);
  PyObject* path;
  if (!is_buffer) {
    path = PyOS_FSPath(arg);
    if (path == nullptr) return 0;
  } else {
    path = arg;
    Py_INCREF(arg);
  }

  PyObject* output;
  if (PyUnicode_Check(path)) {
    output = path;
  } else if (PyBytes_Check(path) || is_buffer) {
    if (!PyBytes_Check(path) &&
        PyErr_WarnFormat(
            PyExc_DeprecationWarning, 1,
            "path should be string, bytes, or os.PathLike, not %.200s",
            PyObject_TypeName(arg))) {
      Py_DECREF(path);
      return 0;
    }
    PyObject* path_bytes = PyBytes_FromObject(path);
    Py_DECREF(path);
    if (!path_bytes) return 0;
    output = PyUnicode_DecodeFSDefaultAndSize(PyBytes_AS_STRING(path_bytes),
                                              PyBytes_GET_SIZE(path_bytes));
    Py_DECREF(path_bytes);
    if (!output) return 0;
  } else {
    Thread::current()->raiseWithFmt(
        LayoutId::kTypeError,
        "path should be string, bytes, or os.PathLike, not %s",
        PyObject_TypeName(arg));
    Py_DECREF(path);
    return 0;
  }

  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Str output_str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(output)));
  if (strFindAsciiChar(output_str, '\0') >= 0) {
    thread->raiseWithFmt(LayoutId::kValueError, "embedded null character");
    Py_DECREF(output);
    return 0;
  }
  *reinterpret_cast<PyObject**>(addr) = output;
  return Py_CLEANUP_SUPPORTED;
}

PY_EXPORT Py_ssize_t PyUnicode_Find(PyObject* str, PyObject* substr,
                                    Py_ssize_t start, Py_ssize_t end,
                                    int direction) {
  DCHECK(str != nullptr, "str must be non-null");
  DCHECK(substr != nullptr, "substr must be non-null");
  DCHECK(direction == -1 || direction == 1, "direction must be -1 or 1");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object haystack_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object needle_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(substr)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*haystack_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError,
                         "PyUnicode_Find requires a 'str' instance");
    return -2;
  }
  Str haystack(&scope, strUnderlying(*haystack_obj));
  if (!runtime->isInstanceOfStr(*needle_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError,
                         "PyUnicode_Find requires a 'str' instance");
    return -2;
  }
  Str needle(&scope, strUnderlying(*needle_obj));
  if (direction == 1) return strFindWithRange(haystack, needle, start, end);
  return strRFind(haystack, needle, start, end);
}

PY_EXPORT Py_ssize_t PyUnicode_FindChar(PyObject* str, Py_UCS4 ch,
                                        Py_ssize_t start, Py_ssize_t end,
                                        int direction) {
  DCHECK(str != nullptr, "str must not be null");
  DCHECK(direction == 1 || direction == -1, "direction must be -1 or 1");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object haystack_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Runtime* runtime = thread->runtime();
  DCHECK(runtime->isInstanceOfStr(*haystack_obj),
         "PyUnicode_FindChar requires a 'str' instance");
  Str haystack(&scope, strUnderlying(*haystack_obj));
  Str needle(&scope, SmallStr::fromCodePoint(ch));
  if (direction == 1) return strFindWithRange(haystack, needle, start, end);
  return strRFind(haystack, needle, start, end);
}

PY_EXPORT PyObject* PyUnicode_Format(PyObject* format, PyObject* args) {
  if (format == nullptr || args == nullptr) {
    PyErr_BadInternalCall();
    return nullptr;
  }
  if (!PyUnicode_Check(format)) {
    Thread::current()->raiseWithFmt(LayoutId::kTypeError, "must be str, not %s",
                                    _PyType_Name(Py_TYPE(format)));
    return nullptr;
  }
  return PyNumber_Remainder(format, args);
}

PY_EXPORT PyObject* PyUnicode_FromEncodedObject(PyObject* /* j */,
                                                const char* /* g */,
                                                const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_FromEncodedObject");
}

PY_EXPORT PyObject* PyUnicode_FromFormat(const char* format, ...) {
  va_list vargs;

  va_start(vargs, format);
  PyObject* ret = PyUnicode_FromFormatV(format, vargs);
  va_end(vargs);
  return ret;
}

PY_EXPORT PyObject* PyUnicode_FromFormatV(const char* format, va_list vargs) {
  va_list vargs2;
  _PyUnicodeWriter writer;

  _PyUnicodeWriter_Init(&writer);
  writer.min_length = std::strlen(format) + 100;
  writer.overallocate = 1;

  // This copy seems unnecessary but it may have been needed by CPython for
  // historical reasons.
  va_copy(vargs2, vargs);

  for (const char* f = format; *f;) {
    if (*f == '%') {
      f = writeArg(&writer, f, &vargs2);
      if (f == nullptr) goto fail;
    } else {
      const char* p = f;
      do {
        if (static_cast<unsigned char>(*p) > 127) {
          PyErr_Format(
              PyExc_ValueError,
              "PyUnicode_FromFormatV() expects an ASCII-encoded format "
              "string, got a non-ASCII byte: 0x%02x",
              static_cast<unsigned char>(*p));
          goto fail;
        }
        p++;
      } while (*p != '\0' && *p != '%');
      Py_ssize_t len = p - f;

      if (*p == '\0') writer.overallocate = 0;

      if (_PyUnicodeWriter_WriteASCIIString(&writer, f, len) < 0) goto fail;

      f = p;
    }
  }
  va_end(vargs2);
  return _PyUnicodeWriter_Finish(&writer);

fail:
  va_end(vargs2);
  _PyUnicodeWriter_Dealloc(&writer);
  return nullptr;
}

PY_EXPORT PyObject* PyUnicode_FromObject(PyObject* /* j */) {
  UNIMPLEMENTED("PyUnicode_FromObject");
}

PY_EXPORT PyObject* PyUnicode_FromOrdinal(int ordinal) {
  Thread* thread = Thread::current();
  if (ordinal < 0 || ordinal > kMaxUnicode) {
    thread->raiseWithFmt(LayoutId::kValueError,
                         "chr() arg not in range(0x110000)");
    return nullptr;
  }
  return ApiHandle::newReference(thread->runtime(),
                                 SmallStr::fromCodePoint(ordinal));
}

PY_EXPORT PyObject* PyUnicode_FromWideChar(const wchar_t* buffer,
                                           Py_ssize_t size) {
  Thread* thread = Thread::current();
  if (buffer == nullptr && size != 0) {
    thread->raiseBadInternalCall();
    return nullptr;
  }

  RawObject result = size == -1
                         ? newStrFromWideChar(thread, buffer)
                         : newStrFromWideCharWithLength(thread, buffer, size);
  return result.isErrorException()
             ? nullptr
             : ApiHandle::newReference(thread->runtime(), result);
}

PY_EXPORT Py_ssize_t PyUnicode_GET_LENGTH_Func(PyObject* pyobj) {
  RawObject obj = ApiHandle::asObjectNoImmediate(ApiHandle::fromPyObject(pyobj));
  DCHECK(Thread::current()->runtime()->isInstanceOfStr(obj),
         "non-str argument to PyUnicode_GET_LENGTH");
  return strUnderlying(obj).codePointLength();
}

PY_EXPORT const char* PyUnicode_GetDefaultEncoding() {
  return Py_FileSystemDefaultEncoding;
}

PY_EXPORT Py_ssize_t PyUnicode_GetLength(PyObject* pyobj) {
  Thread* thread = Thread::current();
  RawObject obj = ApiHandle::asObject(ApiHandle::fromPyObject(pyobj));
  if (!thread->runtime()->isInstanceOfStr(obj)) {
    thread->raiseBadArgument();
    return -1;
  }
  return strUnderlying(obj).codePointLength();
}

PY_EXPORT Py_ssize_t PyUnicode_GetSize(PyObject* pyobj) {
  // This function returns the number of UTF-16 or UTF-32 code units, depending
  // on the size of wchar_t on the operating system. On the machines that we
  // currently use for testing, this is the same as the number of Unicode code
  // points. This must be modified when we support operating systems with
  // different wchar_t (e.g. Windows).
  return PyUnicode_GetLength(pyobj);
}

PY_EXPORT PyObject* PyUnicode_InternFromString(const char* c_str) {
  DCHECK(c_str != nullptr, "c_str must not be nullptr");
  Thread* thread = Thread::current();
  return ApiHandle::newReference(thread->runtime(),
                                 Runtime::internStrFromCStr(thread, c_str));
}

PY_EXPORT void PyUnicode_InternImmortal(PyObject** /* p */) {
  UNIMPLEMENTED("PyUnicode_InternImmortal");
}

PY_EXPORT void PyUnicode_InternInPlace(PyObject** obj_ptr) {
  PyObject* pobj = *obj_ptr;
  DCHECK(pobj != nullptr, "pobj should not be null");
  if (pobj == nullptr) {
    return;
  }
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(pobj)));
  if (!obj.isLargeStr()) {
    return;
  }
  Object result(&scope, Runtime::internStr(thread, obj));
  if (result != obj) {
    Py_DECREF(pobj);
    *obj_ptr = ApiHandle::newReference(thread->runtime(), *result);
  }
}

PY_EXPORT int PyUnicode_IsIdentifier(PyObject* str) {
  DCHECK(str != nullptr, "str must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  if (str_obj == Str::empty()) {
    return false;
  }
  Object result(&scope, thread->invokeMethodStatic1(LayoutId::kStr,
                                                    ID(isidentifier), str_obj));
  DCHECK(!result.isErrorNotFound(), "could not call str.isidentifier");
  CHECK(!result.isError(), "this function should not error");
  return Bool::cast(*result).value();
}

PY_EXPORT PyObject* PyUnicode_Join(PyObject* sep, PyObject* seq) {
  DCHECK(sep != nullptr, "sep should not be null");
  DCHECK(seq != nullptr, "seq should not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object sep_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(sep)));
  // An optimization to rule out non-str values here to use the further
  // optimization of `strJoinWithTupleOrList`.
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*sep_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError,
                         "separator: expected str instance,"
                         "'%T' found",
                         &sep_obj);
    return nullptr;
  }
  Str sep_str(&scope, strUnderlying(*sep_obj));
  Object seq_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(seq)));
  // An ad-hoc optimization for the case `seq_obj` is a `tuple` or `list`,
  // that can be removed without changing the correctness of PyUnicode_Join.
  Object result(&scope, strJoinWithTupleOrList(thread, sep_str, seq_obj));
  if (result.isUnbound()) {
    result =
        thread->invokeMethodStatic2(LayoutId::kStr, ID(join), sep_str, seq_obj);
  }
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError, "could not call str.join");
    }
    return nullptr;
  }
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT PyObject* PyUnicode_Partition(PyObject* str, PyObject* sep) {
  DCHECK(str != nullptr, "str should not be null");
  DCHECK(sep != nullptr, "sep should not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object sep_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(sep)));
  Object result(&scope, thread->invokeMethodStatic2(
                            LayoutId::kStr, ID(partition), str_obj, sep_obj));
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError,
                           "could not call str.partition");
    }
    return nullptr;
  }
  return ApiHandle::newReference(thread->runtime(), *result);
}

PY_EXPORT PyObject* PyUnicode_RPartition(PyObject* str, PyObject* sep) {
  DCHECK(str != nullptr, "str should not be null");
  DCHECK(sep != nullptr, "sep should not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object sep_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(sep)));
  Object result(&scope, thread->invokeMethodStatic2(
                            LayoutId::kStr, ID(rpartition), str_obj, sep_obj));
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError,
                           "could not call str.rpartition");
    }
    return nullptr;
  }
  return ApiHandle::newReference(thread->runtime(), *result);
}

PY_EXPORT PyObject* PyUnicode_RSplit(PyObject* str, PyObject* sep,
                                     Py_ssize_t maxsplit) {
  DCHECK(str != nullptr, "str must not be null");
  DCHECK(sep != nullptr, "sep must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object sep_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(sep)));
  Runtime* runtime = thread->runtime();
  Object maxsplit_obj(&scope, runtime->newInt(maxsplit));
  Object result(&scope,
                thread->invokeMethodStatic3(LayoutId::kStr, ID(rsplit), str_obj,
                                            sep_obj, maxsplit_obj));
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError, "could not call str.rsplit");
    }
    return nullptr;
  }
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT Py_UCS4 PyUnicode_ReadChar(PyObject* obj, Py_ssize_t index) {
  DCHECK(obj != nullptr, "obj must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  if (!runtime->isInstanceOfStr(*str_obj)) {
    thread->raiseBadArgument();
    return -1;
  }
  Str str(&scope, strUnderlying(*str_obj));
  word byte_offset;
  if (index < 0 ||
      (byte_offset = thread->strOffset(str, index)) >= str.length()) {
    thread->raiseWithFmt(LayoutId::kIndexError, "string index out of range");
    return -1;
  }
  word num_bytes;
  return str.codePointAt(byte_offset, &num_bytes);
}

PY_EXPORT PyObject* PyUnicode_Replace(PyObject* str, PyObject* substr,
                                      PyObject* replstr, Py_ssize_t maxcount) {
  DCHECK(str != nullptr, "str must not be null");
  DCHECK(substr != nullptr, "substr must not be null");
  DCHECK(replstr != nullptr, "replstr must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  if (!runtime->isInstanceOfStr(*str_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError, "str must be str");
    return nullptr;
  }

  Object substr_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(substr)));
  if (!runtime->isInstanceOfStr(*substr_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError, "substr must be str");
    return nullptr;
  }

  Object replstr_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(replstr)));
  if (!runtime->isInstanceOfStr(*replstr_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError, "replstr must be str");
    return nullptr;
  }

  Str str_str(&scope, strUnderlying(*str_obj));
  Str substr_str(&scope, strUnderlying(*substr_obj));
  Str replstr_str(&scope, strUnderlying(*replstr_obj));
  return ApiHandle::newReference(
      runtime,
      runtime->strReplace(thread, str_str, substr_str, replstr_str, maxcount));
}

PY_EXPORT int PyUnicode_Resize(PyObject** /* p_unicode */, Py_ssize_t /* h */) {
  UNIMPLEMENTED("PyUnicode_Resize");
}

PY_EXPORT PyObject* PyUnicode_RichCompare(PyObject* /* t */, PyObject* /* t */,
                                          int /* p */) {
  UNIMPLEMENTED("PyUnicode_RichCompare");
}

PY_EXPORT PyObject* PyUnicode_Split(PyObject* str, PyObject* sep,
                                    Py_ssize_t maxsplit) {
  DCHECK(str != nullptr, "str must not be null");
  DCHECK(sep != nullptr, "sep must not be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object sep_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(sep)));
  Runtime* runtime = thread->runtime();
  Object maxsplit_obj(&scope, runtime->newInt(maxsplit));
  Object result(&scope,
                thread->invokeMethodStatic3(LayoutId::kStr, ID(split), str_obj,
                                            sep_obj, maxsplit_obj));
  if (result.isError()) {
    if (result.isErrorNotFound()) {
      thread->raiseWithFmt(LayoutId::kTypeError, "could not call str.split");
    }
    return nullptr;
  }
  return ApiHandle::newReference(runtime, *result);
}

PY_EXPORT PyObject* PyUnicode_Splitlines(PyObject* str, int keepends) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*str_obj)) {
    thread->raiseWithFmt(LayoutId::kTypeError, "must be str, not '%T'",
                         &str_obj);
    return nullptr;
  }
  Str str_str(&scope, strUnderlying(*str_obj));
  return ApiHandle::newReference(runtime,
                                 strSplitlines(thread, str_str, keepends));
}

PY_EXPORT PyObject* PyUnicode_Substring(PyObject* pyobj, Py_ssize_t start,
                                        Py_ssize_t end) {
  DCHECK(pyobj != nullptr, "null argument to PyUnicode_Substring");
  Thread* thread = Thread::current();
  if (start < 0 || end < 0) {
    thread->raiseWithFmt(LayoutId::kIndexError, "string index out of range");
    return nullptr;
  }
  Runtime* runtime = thread->runtime();
  if (end <= start) {
    return ApiHandle::newReference(runtime, Str::empty());
  }
  HandleScope scope(thread);
  ApiHandle* handle = ApiHandle::fromPyObject(pyobj);
  Object obj(&scope, ApiHandle::asObject(handle));
  DCHECK(runtime->isInstanceOfStr(*obj),
         "PyUnicode_Substring requires a 'str' instance");
  Str self(&scope, strUnderlying(*obj));
  word len = self.length();
  word start_index = thread->strOffset(self, start);
  if (start_index == len) {
    return ApiHandle::newReference(runtime, Str::empty());
  }
  word end_index = thread->strOffset(self, end);
  if (end_index == len) {
    if (start_index == 0) {
      ApiHandle::incref(handle);
      return pyobj;
    }
  }
  return ApiHandle::newReference(
      runtime, strSubstr(thread, self, start_index, end_index - start_index));
}

PY_EXPORT Py_ssize_t PyUnicode_Tailmatch(PyObject* str, PyObject* substr,
                                         Py_ssize_t start, Py_ssize_t end,
                                         int direction) {
  DCHECK(str != nullptr, "str must be non-null");
  DCHECK(substr != nullptr, "substr must be non-null");
  DCHECK(direction == -1 || direction == 1, "direction must be -1 or 1");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object haystack_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(str)));
  Object needle_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(substr)));
  Runtime* runtime = thread->runtime();
  if (!runtime->isInstanceOfStr(*haystack_obj) ||
      !runtime->isInstanceOfStr(*needle_obj)) {
    thread->raiseBadArgument();
    return -1;
  }
  Str haystack(&scope, strUnderlying(*haystack_obj));
  Str needle(&scope, strUnderlying(*needle_obj));
  word haystack_len = haystack.codePointLength();
  Slice::adjustSearchIndices(&start, &end, haystack_len);
  word needle_len = needle.codePointLength();
  if (start + needle_len > end) {
    return 0;
  }
  word start_offset;
  if (direction == 1) {
    start_offset = haystack.offsetByCodePoints(0, end - needle_len);
  } else {
    start_offset = haystack.offsetByCodePoints(0, start);
  }
  word needle_chars = needle.length();
  for (word i = start_offset, j = 0; j < needle_chars; i++, j++) {
    if (haystack.byteAt(i) != needle.byteAt(j)) {
      return 0;
    }
  }
  return 1;
}

PY_EXPORT PyObject* PyUnicode_Translate(PyObject* /* r */, PyObject* /* g */,
                                        const char* /* s */) {
  UNIMPLEMENTED("PyUnicode_Translate");
}

PY_EXPORT PyTypeObject* PyUnicode_Type_Ptr() {
  Runtime* runtime = Thread::current()->runtime();
  return reinterpret_cast<PyTypeObject*>(
      ApiHandle::borrowedReference(runtime, runtime->typeAt(LayoutId::kStr)));
}

PY_EXPORT int PyUnicode_WriteChar(PyObject* /* e */, Py_ssize_t /* x */,
                                  Py_UCS4 /* h */) {
  UNIMPLEMENTED("PyUnicode_WriteChar");
}

PY_EXPORT Py_UNICODE* PyUnicode_AsUnicode(PyObject* /* e */) {
  UNIMPLEMENTED("PyUnicode_AsUnicode");
}

PY_EXPORT Py_UNICODE* PyUnicode_AsUnicodeAndSize(PyObject* /* unicode */,
                                                 Py_ssize_t* /* size */) {
  UNIMPLEMENTED("PyUnicode_AsUnicodeAndSize");
}

template <typename T>
static PyObject* decodeUnicodeToString(Thread* thread, const void* src,
                                       word size) {
  Runtime* runtime = thread->runtime();
  DCHECK(src != nullptr, "Must pass in a non-null buffer");
  const T* cp = static_cast<const T*>(src);
  if (size == 1) {
    return ApiHandle::newReference(runtime, SmallStr::fromCodePoint(cp[0]));
  }
  HandleScope scope(thread);
  // TODO(T41785453): Remove the StrArray intermediary
  StrArray array(&scope, runtime->newStrArray());
  runtime->strArrayEnsureCapacity(thread, array, size);
  for (word i = 0; i < size; ++i) {
    runtime->strArrayAddCodePoint(thread, array, cp[i]);
  }
  return ApiHandle::newReference(runtime, runtime->strFromStrArray(array));
}

PY_EXPORT PyObject* PyUnicode_FromKindAndData(int kind, const void* buffer,
                                              Py_ssize_t size) {
  Thread* thread = Thread::current();
  if (size < 0) {
    thread->raiseWithFmt(LayoutId::kValueError, "size must be positive");
    return nullptr;
  }
  if (size == 0) {
    return ApiHandle::newReference(thread->runtime(), Str::empty());
  }
  switch (kind) {
    case PyUnicode_1BYTE_KIND:
      return decodeUnicodeToString<Py_UCS1>(thread, buffer, size);
    case PyUnicode_2BYTE_KIND:
      return decodeUnicodeToString<Py_UCS2>(thread, buffer, size);
    case PyUnicode_4BYTE_KIND:
      return decodeUnicodeToString<Py_UCS4>(thread, buffer, size);
  }
  thread->raiseWithFmt(LayoutId::kSystemError, "invalid kind");
  return nullptr;
}

PY_EXPORT PyObject* PyUnicode_FromUnicode(const Py_UNICODE* code_units,
                                          Py_ssize_t size) {
  if (code_units == nullptr) {
    // TODO(T36562134): Implement _PyUnicode_New
    UNIMPLEMENTED("_PyUnicode_New");
  }

  Thread* thread = Thread::current();
  RawObject result = newStrFromWideCharWithLength(thread, code_units, size);
  return result.isErrorException()
             ? nullptr
             : ApiHandle::newReference(thread->runtime(), result);
}

PY_EXPORT int PyUnicode_KIND_Func(PyObject* obj) {
  // TODO(T47682853): Introduce new PyUnicode_VARBYTE_KIND
  CHECK(PyUnicode_IS_ASCII_Func(obj), "only ASCII allowed");
  return PyUnicode_1BYTE_KIND;
}

// NOTE: This will return a cached and managed C-string buffer that is a copy
// of the Str internal buffer. It is NOT a direct pointer into the string
// object, so writing into this buffer will do nothing. This is different
// behavior from CPython, where changing the data in the buffer changes the
// string object.
PY_EXPORT void* PyUnicode_DATA_Func(PyObject* str) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  ApiHandle* handle = ApiHandle::fromPyObject(str);
  if (void* cache = ApiHandle::cache(runtime, handle)) {
    return static_cast<char*>(cache);
  }
  HandleScope scope(thread);
  Object obj(&scope, ApiHandle::asObject(handle));
  DCHECK(runtime->isInstanceOfStr(*obj), "str should be a str instance");
  Str str_obj(&scope, strUnderlying(*obj));
  word length = str_obj.length();
  byte* result = static_cast<byte*>(std::malloc(length + 1));
  str_obj.copyTo(result, length);
  result[length] = '\0';
  ApiHandle::setCache(runtime, handle, result);
  ApiHandle::setBorrowedNoImmediate(handle);
  return reinterpret_cast<char*>(result);
}

PY_EXPORT Py_UCS4 PyUnicode_READ_Func(int kind, void* data, Py_ssize_t index) {
  if (kind == PyUnicode_1BYTE_KIND) return static_cast<Py_UCS1*>(data)[index];
  if (kind == PyUnicode_2BYTE_KIND) return static_cast<Py_UCS2*>(data)[index];
  DCHECK(kind == PyUnicode_4BYTE_KIND, "kind must be PyUnicode_4BYTE_KIND");
  return static_cast<Py_UCS4*>(data)[index];
}

PY_EXPORT Py_UCS4 PyUnicode_READ_CHAR_Func(PyObject* obj, Py_ssize_t index) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  DCHECK(thread->runtime()->isInstanceOfStr(*str_obj),
         "PyUnicode_READ_CHAR must receive a unicode object");
  Str str(&scope, strUnderlying(*str_obj));
  word byte_offset = thread->strOffset(str, index);
  if (byte_offset == str.length()) return Py_UCS4{0};
  word num_bytes;
  return static_cast<Py_UCS4>(str.codePointAt(byte_offset, &num_bytes));
}

PY_EXPORT int PyUnicode_IS_ASCII_Func(PyObject* obj) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Object str(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(obj)));
  DCHECK(thread->runtime()->isInstanceOfStr(*str),
         "strIsASCII must receive a unicode object");
  return strUnderlying(*str).isASCII() ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISALPHA_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isAlpha(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISDECIMAL_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isDecimal(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISDIGIT_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isDigit(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISLINEBREAK_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isLinebreak(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISLOWER_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isLower(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISNUMERIC_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isNumeric(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISPRINTABLE_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isPrintable(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISSPACE_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isSpace(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISTITLE_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isTitle(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_ISUPPER_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return 0;
  }
  return Unicode::isUpper(static_cast<int32_t>(code_point)) ? 1 : 0;
}

PY_EXPORT int Py_UNICODE_TODECIMAL_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return -1;
  }
  return Unicode::toDecimal(static_cast<int32_t>(code_point));
}

PY_EXPORT int Py_UNICODE_TODIGIT_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return -1;
  }
  return Unicode::toDigit(static_cast<int32_t>(code_point));
}

PY_EXPORT Py_UCS4 Py_UNICODE_TOLOWER_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return code_point;
  }
  FullCasing lower = Unicode::toLower(static_cast<int32_t>(code_point));
  return lower.code_points[0];
}

PY_EXPORT double Py_UNICODE_TONUMERIC_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return -1.0;
  }
  return Unicode::toNumeric(static_cast<int32_t>(code_point));
}

PY_EXPORT Py_UCS4 Py_UNICODE_TOTITLE_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return code_point;
  }
  FullCasing title = Unicode::toTitle(static_cast<int32_t>(code_point));
  return title.code_points[0];
}

PY_EXPORT Py_UCS4 Py_UNICODE_TOUPPER_Func(Py_UCS4 code_point) {
  if (code_point > kMaxUnicode) {
    return code_point;
  }
  FullCasing upper = Unicode::toUpper(static_cast<int32_t>(code_point));
  return upper.code_points[0];
}

PY_EXPORT int _Py_normalize_encoding(const char* encoding, char* lower,
                                     size_t lower_len) {
  char* buffer = lower;
  const char* lower_end = &lower[lower_len - 1];
  bool has_punct = false;
  for (char ch = *encoding; ch != '\0'; ch = *++encoding) {
    if (Py_ISALNUM(ch) || ch == '.') {
      if (has_punct && buffer != lower) {
        if (buffer == lower_end) {
          return 0;
        }
        *buffer++ = '_';
      }
      has_punct = false;

      if (buffer == lower_end) {
        return 0;
      }
      *buffer++ = Py_TOLOWER(ch);
    } else {
      has_punct = true;
    }
  }
  *buffer = '\0';
  return 1;
}

PY_EXPORT PyObject* _PyUnicode_AsUTF8String(PyObject* unicode,
                                            const char* errors) {
  DCHECK(unicode != nullptr, "unicode cannot be null");
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Runtime* runtime = thread->runtime();
  Object obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(unicode)));
  if (!runtime->isInstanceOfStr(*obj)) {
    thread->raiseBadArgument();
    return nullptr;
  }
  Str str(&scope, strUnderlying(*obj));
  if (!strHasSurrogate(str)) {
    word length = str.length();
    MutableBytes result(&scope, runtime->newMutableBytesUninitialized(length));
    result.replaceFromWithStr(0, *str, length);
    return ApiHandle::newReference(runtime, result.becomeImmutable());
  }
  Object errors_obj(&scope, symbolFromError(thread, errors));
  Object tuple_obj(&scope, thread->invokeFunction2(
                               ID(_codecs), ID(utf_8_encode), str, errors_obj));
  if (tuple_obj.isError()) {
    return nullptr;
  }
  Tuple tuple(&scope, *tuple_obj);
  return ApiHandle::newReference(runtime, tuple.at(0));
}

PY_EXPORT wchar_t* _Py_DecodeUTF8_surrogateescape(const char* c_str,
                                                  Py_ssize_t size,
                                                  size_t* wlen) {
  DCHECK(c_str != nullptr, "c_str cannot be null");
  wchar_t* wc_str =
      static_cast<wchar_t*>(PyMem_RawMalloc((size + 1) * sizeof(wchar_t)));
  for (Py_ssize_t i = 0; i < size; i++) {
    char ch = c_str[i];
    // TODO(T57811636): Support UTF-8 arguments on macOS.
    // We don't have UTF-8 decoding machinery that is decoupled from the
    // runtime
    if (ch & 0x80) {
      UNIMPLEMENTED("UTF-8 argument support unimplemented");
    }
    wc_str[i] = static_cast<wchar_t>(ch);
  }
  wc_str[size] = '\0';
  if (wlen != nullptr) {
    *wlen = size;
  }
  return wc_str;
}

PY_EXPORT int _Py_DecodeUTF8Ex(const char* c_str, Py_ssize_t size,
                               wchar_t** result, size_t* wlen,
                               const char** /* reason */,
                               _Py_error_handler /* surrogateescape */) {
  wchar_t* wc_str =
      static_cast<wchar_t*>(PyMem_RawMalloc((size + 1) * sizeof(*wc_str)));
  if (wc_str == nullptr) {
    return -1;
  }
  for (Py_ssize_t i = 0; i < size; i++) {
    byte ch = c_str[i];
    // TODO(T57811636): Support UTF-8 decoding decoupled from the runtime.
    // We don't have UTF-8 decoding machinery that is decoupled from the
    // runtime
    if (ch > kMaxASCII) {
      UNIMPLEMENTED("UTF-8 argument support unimplemented");
    }
    wc_str[i] = ch;
  }
  wc_str[size] = '\0';
  *result = wc_str;
  if (wlen) {
    *wlen = size;
  }
  return 0;
}

// UTF-8 encoder using the surrogateescape error handler .
//
// On success, return 0 and write the newly allocated character string (use
// PyMem_Free() to free the memory) into *str.
//
// On encoding failure, return -2 and write the position of the invalid
// surrogate character into *error_pos (if error_pos is set) and the decoding
// error message into *reason (if reason is set).
//
// On memory allocation failure, return -1.
PY_EXPORT int _Py_EncodeUTF8Ex(const wchar_t* text, char** str,
                               size_t* error_pos, const char** reason,
                               int raw_malloc, _Py_error_handler errors) {
  const Py_ssize_t max_char_size = 4;
  Py_ssize_t len = std::wcslen(text);
  DCHECK(len >= 0, "len must be non-negative");

  bool surrogateescape = false;
  bool surrogatepass = false;
  switch (errors) {
    case _Py_ERROR_STRICT:
      break;
    case _Py_ERROR_SURROGATEESCAPE:
      surrogateescape = true;
      break;
    case _Py_ERROR_SURROGATEPASS:
      surrogatepass = true;
      break;
    default:
      return -3;
  }

  if (len > PY_SSIZE_T_MAX / max_char_size - 1) {
    return -1;
  }
  char* bytes;
  if (raw_malloc) {
    bytes = reinterpret_cast<char*>(PyMem_RawMalloc((len + 1) * max_char_size));
  } else {
    bytes = reinterpret_cast<char*>(PyMem_Malloc((len + 1) * max_char_size));
  }
  if (bytes == nullptr) {
    return -1;
  }

  char* p = bytes;
  for (Py_ssize_t i = 0; i < len; i++) {
    Py_UCS4 ch = text[i];

    if (ch < 0x80) {
      // Encode ASCII
      *p++ = (char)ch;

    } else if (ch < 0x0800) {
      // Encode Latin-1
      *p++ = (char)(0xc0 | (ch >> 6));
      *p++ = (char)(0x80 | (ch & 0x3f));
    } else if (Py_UNICODE_IS_SURROGATE(ch) && !surrogatepass) {
      // surrogateescape error handler
      if (!surrogateescape || !(0xDC80 <= ch && ch <= 0xDCFF)) {
        if (error_pos != nullptr) {
          *error_pos = (size_t)i;
        }
        if (reason != nullptr) {
          *reason = "encoding error";
        }
        if (raw_malloc) {
          PyMem_RawFree(bytes);
        } else {
          PyMem_Free(bytes);
        }
        return -2;
      }
      *p++ = (char)(ch & 0xff);
    } else if (ch < 0x10000) {
      *p++ = (char)(0xe0 | (ch >> 12));
      *p++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *p++ = (char)(0x80 | (ch & 0x3f));
    } else {
      // ch >= 0x10000
      DCHECK(ch <= kMaxUnicode, "ch must be a valid unicode code point");
      // Encode UCS4 Unicode ordinals
      *p++ = (char)(0xf0 | (ch >> 18));
      *p++ = (char)(0x80 | ((ch >> 12) & 0x3f));
      *p++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *p++ = (char)(0x80 | (ch & 0x3f));
    }
  }
  *p++ = '\0';

  size_t final_size = (p - bytes);
  char* bytes2;
  if (raw_malloc) {
    bytes2 = reinterpret_cast<char*>(PyMem_RawRealloc(bytes, final_size));
  } else {
    bytes2 = reinterpret_cast<char*>(PyMem_Realloc(bytes, final_size));
  }
  if (bytes2 == nullptr) {
    if (error_pos != nullptr) {
      *error_pos = (size_t)-1;
    }
    if (raw_malloc) {
      PyMem_RawFree(bytes);
    } else {
      PyMem_Free(bytes);
    }
    return -1;
  }
  *str = bytes2;
  return 0;
}

}  // namespace py
