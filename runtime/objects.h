/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
// Object structures
// -----------------
//
// This file describes the layout of built-in types on the managed heap. There
// is one class for each type providing an abstraction over a simple pointer.
// Each class provides constants for field offsets and a getter and setter
// function for each field.
//
// There should be a public getter and setter for every field using RawObject
// or word types. The code is meant to abstract over the layout, not to provide
// abstract data types: Do not add further type or consistency checks and leave
// that to higher levels.
#pragma once

#include <cstdio>
#include <limits>

#include "globals.h"
#include "utils.h"
#include "view.h"

namespace py {

class Byteslike;
class Frame;
template <typename T>
class Handle;
class Thread;

// Python types that store their value directly in a RawObject.
#define IMMEDIATE_CLASS_NAMES(V)                                               \
  V(SmallInt)                                                                  \
  V(SmallBytes)                                                                \
  V(SmallStr)                                                                  \
  V(Bool)                                                                      \
  V(NotImplementedType)                                                        \
  V(Unbound)                                                                   \
  V(NoneType)

// Python types that hold a pointer to heap-allocated data in a RawObject.
// Subclasses of `RawInstance` are listed separately in `INSTANCE_CLASS_NAMES`.
#define HEAP_CLASS_NAMES(V)                                                    \
  V(Bytes)                                                                     \
  V(Complex)                                                                   \
  V(Ellipsis)                                                                  \
  V(Float)                                                                     \
  V(Int)                                                                       \
  V(LargeBytes)                                                                \
  V(LargeInt)                                                                  \
  V(LargeStr)                                                                  \
  V(MutableBytes)                                                              \
  V(MutableTuple)                                                              \
  V(Str)                                                                       \
  V(Tuple)

#define INSTANCE_CLASS_NAMES(V)                                                \
  V(Array)                                                                     \
  V(AsyncGenerator)                                                            \
  V(AsyncGeneratorAclose)                                                      \
  V(AsyncGeneratorAsend)                                                       \
  V(AsyncGeneratorAthrow)                                                      \
  V(AsyncGeneratorOpIterBase)                                                  \
  V(AsyncGeneratorWrappedValue)                                                \
  V(BoundMethod)                                                               \
  V(BufferedRandom)                                                            \
  V(BufferedReader)                                                            \
  V(BufferedWriter)                                                            \
  V(Bytearray)                                                                 \
  V(BytearrayIterator)                                                         \
  V(BytesIO)                                                                   \
  V(BytesIterator)                                                             \
  V(Cell)                                                                      \
  V(ClassMethod)                                                               \
  V(Code)                                                                      \
  V(Context)                                                                   \
  V(ContextVar)                                                                \
  V(Coroutine)                                                                 \
  V(CoroutineWrapper)                                                          \
  V(Deque)                                                                     \
  V(DequeIterator)                                                             \
  V(DequeReverseIterator)                                                      \
  V(Dict)                                                                      \
  V(DictItemIterator)                                                          \
  V(DictItems)                                                                 \
  V(DictKeyIterator)                                                           \
  V(DictKeys)                                                                  \
  V(DictValueIterator)                                                         \
  V(DictValues)                                                                \
  V(Enumerate)                                                                 \
  V(ExceptionState)                                                            \
  V(FileIO)                                                                    \
  V(FrameProxy)                                                                \
  V(FrozenSet)                                                                 \
  V(Function)                                                                  \
  V(Generator)                                                                 \
  V(GeneratorFrame)                                                            \
  V(IncrementalNewlineDecoder)                                                 \
  V(InstanceMethod)                                                            \
  V(InstanceProxy)                                                             \
  V(Layout)                                                                    \
  V(List)                                                                      \
  V(ListIterator)                                                              \
  V(LongRangeIterator)                                                         \
  V(MappingProxy)                                                              \
  V(MemoryView)                                                                \
  V(Mmap)                                                                      \
  V(Module)                                                                    \
  V(ModuleProxy)                                                               \
  V(Object)                                                                    \
  V(Pointer)                                                                   \
  V(Property)                                                                  \
  V(Range)                                                                     \
  V(RangeIterator)                                                             \
  V(SeqIterator)                                                               \
  V(Set)                                                                       \
  V(SetIterator)                                                               \
  V(Slice)                                                                     \
  V(SlotDescriptor)                                                            \
  V(StaticMethod)                                                              \
  V(StrArray)                                                                  \
  V(StrIterator)                                                               \
  V(StringIO)                                                                  \
  V(Super)                                                                     \
  V(TextIOWrapper)                                                             \
  V(Token)                                                                     \
  V(Traceback)                                                                 \
  V(TupleIterator)                                                             \
  V(Type)                                                                      \
  V(TypeProxy)                                                                 \
  V(UnderBufferedIOBase)                                                       \
  V(UnderBufferedIOMixin)                                                      \
  V(UnderIOBase)                                                               \
  V(UnderRawIOBase)                                                            \
  V(UnderTextIOBase)                                                           \
  V(ValueCell)                                                                 \
  V(WeakCallableProxy)                                                         \
  V(WeakProxy)                                                                 \
  V(WeakLink)                                                                  \
  V(WeakRef)

// Heap-allocated Python types in the BaseException hierarchy.
#define EXCEPTION_CLASS_NAMES(V)                                               \
  V(ArithmeticError)                                                           \
  V(AssertionError)                                                            \
  V(AttributeError)                                                            \
  V(BaseException)                                                             \
  V(BlockingIOError)                                                           \
  V(BrokenPipeError)                                                           \
  V(BufferError)                                                               \
  V(BytesWarning)                                                              \
  V(ChildProcessError)                                                         \
  V(ConnectionAbortedError)                                                    \
  V(ConnectionError)                                                           \
  V(ConnectionRefusedError)                                                    \
  V(ConnectionResetError)                                                      \
  V(DeprecationWarning)                                                        \
  V(EOFError)                                                                  \
  V(Exception)                                                                 \
  V(FileExistsError)                                                           \
  V(FileNotFoundError)                                                         \
  V(FloatingPointError)                                                        \
  V(FutureWarning)                                                             \
  V(GeneratorExit)                                                             \
  V(ImportError)                                                               \
  V(ImportWarning)                                                             \
  V(IndentationError)                                                          \
  V(IndexError)                                                                \
  V(InterruptedError)                                                          \
  V(IsADirectoryError)                                                         \
  V(KeyboardInterrupt)                                                         \
  V(KeyError)                                                                  \
  V(LookupError)                                                               \
  V(MemoryError)                                                               \
  V(ModuleNotFoundError)                                                       \
  V(NameError)                                                                 \
  V(NotADirectoryError)                                                        \
  V(NotImplementedError)                                                       \
  V(OSError)                                                                   \
  V(OverflowError)                                                             \
  V(PendingDeprecationWarning)                                                 \
  V(PermissionError)                                                           \
  V(ProcessLookupError)                                                        \
  V(RecursionError)                                                            \
  V(ReferenceError)                                                            \
  V(ResourceWarning)                                                           \
  V(RuntimeError)                                                              \
  V(RuntimeWarning)                                                            \
  V(StopAsyncIteration)                                                        \
  V(StopIteration)                                                             \
  V(SyntaxError)                                                               \
  V(SyntaxWarning)                                                             \
  V(SystemError)                                                               \
  V(SystemExit)                                                                \
  V(TabError)                                                                  \
  V(TimeoutError)                                                              \
  V(TypeError)                                                                 \
  V(UnboundLocalError)                                                         \
  V(UnicodeDecodeError)                                                        \
  V(UnicodeEncodeError)                                                        \
  V(UnicodeError)                                                              \
  V(UnicodeTranslateError)                                                     \
  V(UnicodeWarning)                                                            \
  V(UserWarning)                                                               \
  V(ValueError)                                                                \
  V(Warning)                                                                   \
  V(ZeroDivisionError)

#define CLASS_NAMES(V)                                                         \
  IMMEDIATE_CLASS_NAMES(V)                                                     \
  HEAP_CLASS_NAMES(V)                                                          \
  INSTANCE_CLASS_NAMES(V)                                                      \
  EXCEPTION_CLASS_NAMES(V)

// This enumerates layout ids of intrinsic classes. Notably, the layout of an
// instance of an intrinsic class does not change.
//
// An instance of an intrinsic class that has an immediate representation
// cannot have attributes added.  An instance of an intrinsic class that is heap
// allocated has a predefined number in-object attributes in the base
// instance.  For some of those types, the language forbids adding new
// attributes.  For the types which are permitted to have attributes added,
// these types must include a hidden attribute that indirects to attribute
// storage.
//
// NB: If you add something here make sure you add it to the appropriate macro
// above
enum class LayoutId : word {
  // Immediate objects - note that the SmallInt class is also aliased to all
  // even integers less than 32, so that classes of immediate objects can be
  // looked up simply by using the low 5 bits of the immediate value. This
  // implies that all other immediate class ids must be odd.
  kSmallInt = 0,
  kSmallBytes = 5,
  kBool = 7,
  kSmallStr = 13,
  kNotImplementedType = 15,
  // There is no RawType associated with the RawError object type, this is here
  // as a placeholder.
  kError = 21,
  kUnbound = 23,
  // We have room for one more immediate object with LayoutId = 29
  kNoneType = 31,

// clang-format off
  // Heap objects
#define LAYOUT_ID(name) k##name,
#define GET_FIRST(name) k##name + 0 *
#define GET_LAST(name) 0 + k##name *
  HEAP_CLASS_NAMES(LAYOUT_ID)
  kLastNonInstance = HEAP_CLASS_NAMES(GET_LAST) 1,

  INSTANCE_CLASS_NAMES(LAYOUT_ID)
  EXCEPTION_CLASS_NAMES(LAYOUT_ID)

  // Mark the first and last Exception LayoutIds, to allow range comparisons.
  kFirstException = EXCEPTION_CLASS_NAMES(GET_FIRST) 0,
  kLastException = EXCEPTION_CLASS_NAMES(GET_LAST) 1,
#undef GET_FIRST
#undef GET_LAST
#undef LAYOUT_ID
  // clang-format on

  kLastBuiltinId = kLastException,
  kSentinelId = kLastBuiltinId + 1,
};

// Add functionality common to all RawObject subclasses, split into two parts
// since some types manually define cast() but want everything else.
#define RAW_OBJECT_COMMON_NO_CAST(ty)                                          \
  /* TODO(T34683229): Once Handle<T> doesn't inherit from T, delete this.      \
   * Right now it exists to prevent implicit conversion of Handle<T> to T. */  \
  template <typename T>                                                        \
  Raw##ty(const Handle<T>&) = delete;                                          \
  DISALLOW_HEAP_ALLOCATION()

#define RAW_OBJECT_COMMON(ty)                                                  \
  static Raw##ty cast(RawObject object) {                                      \
    DCHECK(object.is##ty(), "invalid cast, expected " #ty);                    \
    return object.rawCast<Raw##ty>();                                          \
  }                                                                            \
  RAW_OBJECT_COMMON_NO_CAST(ty)

const int kObjectAlignmentLog2 = 4;  // bits
const int kObjectAlignment = word{1} << kObjectAlignmentLog2;

bool isInstanceLayout(LayoutId id);

word roundAllocationSize(word size);

class RawObject {
 public:
  explicit RawObject(uword raw);

  // Getters and setters.
  uword raw() const;
  bool isObject() const;
  bool isInternal() const;
  LayoutId layoutId() const;

  // Immediate objects
  bool isBool() const;
  bool isError() const;
  bool isErrorError() const;
  bool isErrorException() const;
  bool isErrorNoMoreItems() const;
  bool isErrorNotFound() const;
  bool isErrorOutOfBounds() const;
  bool isErrorOutOfMemory() const;
  bool isHeader() const;
  bool isImmediateObjectNotSmallInt() const;
  bool isNoneType() const;
  bool isNotImplementedType() const;
  bool isSmallBytes() const;
  bool isSmallInt() const;
  bool isSmallStr() const;
  bool isUnbound() const;

  // Heap objects
  bool isArray() const;
  bool isAsyncGenerator() const;
  bool isAsyncGeneratorAclose() const;
  bool isAsyncGeneratorAsend() const;
  bool isAsyncGeneratorAthrow() const;
  bool isAsyncGeneratorOpIterBase() const;
  bool isAsyncGeneratorWrappedValue() const;
  bool isAttributeDict() const;
  bool isBaseException() const;
  bool isBoundMethod() const;
  bool isBufferedRandom() const;
  bool isBufferedReader() const;
  bool isBufferedWriter() const;
  bool isBytearray() const;
  bool isBytearrayIterator() const;
  bool isBytesIO() const;
  bool isBytesIterator() const;
  bool isCell() const;
  bool isClassMethod() const;
  bool isCode() const;
  bool isComplex() const;
  bool isContext() const;
  bool isContextVar() const;
  bool isCoroutine() const;
  bool isCoroutineWrapper() const;
  bool isDataArray() const;
  bool isDeque() const;
  bool isDequeIterator() const;
  bool isDequeReverseIterator() const;
  bool isDict() const;
  bool isDictItemIterator() const;
  bool isDictItems() const;
  bool isDictKeyIterator() const;
  bool isDictKeys() const;
  bool isDictValueIterator() const;
  bool isDictValues() const;
  bool isEllipsis() const;
  bool isEnumerate() const;
  bool isException() const;
  bool isExceptionState() const;
  bool isFileIO() const;
  bool isFloat() const;
  bool isFrameProxy() const;
  bool isFrozenSet() const;
  bool isFunction() const;
  bool isGenerator() const;
  bool isGeneratorFrame() const;
  bool isHeapObject() const;
  bool isHeapObjectWithLayout(LayoutId layout_id) const;
  bool isImportError() const;
  bool isIncrementalNewlineDecoder() const;
  bool isIndexError() const;
  bool isInstance() const;
  bool isInstanceMethod() const;
  bool isInstanceProxy() const;
  bool isKeyError() const;
  bool isLargeBytes() const;
  bool isLargeInt() const;
  bool isLargeStr() const;
  bool isLayout() const;
  bool isList() const;
  bool isListIterator() const;
  bool isLongRangeIterator() const;
  bool isLookupError() const;
  bool isMappingProxy() const;
  bool isMemoryView() const;
  bool isMmap() const;
  bool isModule() const;
  bool isModuleNotFoundError() const;
  bool isModuleProxy() const;
  bool isMutableBytes() const;
  bool isMutableTuple() const;
  bool isNotImplementedError() const;
  bool isPointer() const;
  bool isProperty() const;
  bool isRange() const;
  bool isRangeIterator() const;
  bool isRuntimeError() const;
  bool isSeqIterator() const;
  bool isSet() const;
  bool isSetIterator() const;
  bool isSlice() const;
  bool isSlotDescriptor() const;
  bool isStaticMethod() const;
  bool isStopIteration() const;
  bool isStrArray() const;
  bool isStrIterator() const;
  bool isStringIO() const;
  bool isSuper() const;
  bool isSyntaxError() const;
  bool isSystemExit() const;
  bool isTextIOWrapper() const;
  bool isToken() const;
  bool isTraceback() const;
  bool isTuple() const;
  bool isTupleIterator() const;
  bool isType() const;
  bool isTypeProxy() const;
  bool isUnderBufferedIOBase() const;
  bool isUnderBufferedIOMixin() const;
  bool isUnderIOBase() const;
  bool isUnderRawIOBase() const;
  bool isUnicodeDecodeError() const;
  bool isUnicodeEncodeError() const;
  bool isUnicodeError() const;
  bool isUnicodeErrorBase() const;
  bool isUnicodeTranslateError() const;
  bool isValueCell() const;
  bool isWeakCallableProxy() const;
  bool isWeakProxy() const;
  bool isWeakLink() const;
  bool isWeakRef() const;

  // superclass objects
  bool isBytes() const;
  bool isGeneratorBase() const;
  bool isInt() const;
  bool isSetBase() const;
  bool isStr() const;

  bool operator==(const RawObject& other) const;
  bool operator!=(const RawObject& other) const;

  // Constants

  // Tags.
  static const uword kSmallIntTag = 0;         // 0b****0
  static const uword kHeapObjectTag = 1;       // 0b**001
  static const uword kHeaderTag = 3;           // 0b**011
  static const uword kSmallBytesTag = 5;       // 0b00101
  static const uword kSmallStrTag = 13;        // 0b01101
  static const uword kErrorTag = 21;           // 0b10101
                                               // 0b11101 is unused
  static const uword kBoolTag = 7;             // 0b00111
  static const uword kNotImplementedTag = 15;  // 0b01111
  static const uword kUnboundTag = 23;         // 0b10111
  static const uword kNoneTag = 31;            // 0b11111

  // Up to the five least significant bits are used to tag the object's layout.
  // The three low bits make up a primary tag, used to differentiate Header and
  // HeapObject from immediate objects. All even tags map to SmallInt, which is
  // optimized by checking only the lowest bit for parity.
  static const uword kSmallIntTagBits = 1;
  static const uword kPrimaryTagBits = 3;
  static const uword kImmediateTagBits = 5;
  static const uword kSmallIntTagMask = (1 << kSmallIntTagBits) - 1;
  static const uword kPrimaryTagMask = (1 << kPrimaryTagBits) - 1;
  static const uword kImmediateTagMask = (1 << kImmediateTagBits) - 1;

  // Cast this RawObject to another Raw* type with no runtime checks. Only used
  // in a few limited situations; most code should use Raw*::cast() instead.
  template <typename T>
  T rawCast() const;

  RAW_OBJECT_COMMON(Object);

 private:
  // Zero-initializing raw_ gives RawSmallInt::fromWord(0).
  uword raw_{};
};

// CastError and OptInt<T> represent the result of a call to RawInt::asInt<T>():
// If error == CastError::None, value contains the result. Otherwise, error
// indicates why the value didn't fit in T.
enum class CastError {
  None,
  Underflow,
  Overflow,
};

template <typename T>
class OptInt {
 public:
  static OptInt valid(T i) { return {i, CastError::None}; }
  static OptInt underflow() { return {0, CastError::Underflow}; }
  static OptInt overflow() { return {0, CastError::Overflow}; }

  T value;
  CastError error;
};

// Generic superclasses for Python types with multiple native types

// Common `bytes` wrapper around RawSmallBytes/RawLargeBytes
class RawBytes : public RawObject {
 public:
  // Singleton.
  static RawBytes empty();

  // Getters and setters.
  word length() const;
  byte byteAt(word index) const;
  void copyTo(byte* dst, word length) const;
  // Copy length bytes from this to dst, starting at the given index
  void copyToStartAt(byte* dst, word length, word index) const;
  bool isASCII() const;
  // Read adjacent bytes as `uint16_t` integer.
  uint16_t uint16At(word index) const;
  // Read adjacent bytes as `uint32_t` integer.
  uint32_t uint32At(word index) const;
  // Read adjacent bytes as `uint64_t` integer.
  uint64_t uint64At(word index) const;

  // Rewrite the header to make UTF-8 conformant bytes look like a Str
  RawObject becomeStr() const;

  // Returns a positive value if 'this' is greater than 'that', a negative value
  // if 'this' is less than 'that', and zero if they are the same.
  // Does not guarantee to return -1, 0, or 1.
  word compare(RawBytes that) const;

  // Returns the index at which value is found in this[start:start+length] (not
  // including end), or -1 if not found.
  word findByte(byte value, word start, word length) const;

  // Check for the presence of a non-zero byte.
  bool includesByte(byte b) const;

  // Conversion to an unescaped C string.  The underlying memory is allocated
  // with malloc and must be freed by the caller.
  char* toCStr() const;

  RAW_OBJECT_COMMON(Bytes);
};

// Common `int` wrapper around RawSmallInt/RawLargeInt/RawBool
class RawInt : public RawObject {
 public:
  // Getters and setters.
  word asWord() const;
  // Returns the value as a word if it fits into a word.
  // Otherwise, returns kMinWord for negative values or kMaxWord for positive
  // values.
  word asWordSaturated() const;
  void* asCPtr() const;

  // If this fits in T, get its value as a T. If not, indicate what went wrong.
  template <typename T>
  OptInt<T> asInt() const;

  // Returns a positive value if 'this' is greater than 'other', zero if it
  // is the same, a negavite value if smaller. The value does not have to be
  // -1, 0, or 1.
  word compare(RawInt that) const;

  word bitLength() const;

  bool isEven() const;
  bool isNegative() const;
  bool isOdd() const;
  bool isPositive() const;
  bool isZero() const;

  // Indexing into digits
  uword digitAt(word index) const;

  // Number of digits
  word numDigits() const;

  // Copies digits bytewise to `dst`. Returns number of bytes copied.
  word copyTo(byte* dst, word max_length) const;

  RAW_OBJECT_COMMON(Int);
};

// Common `str` wrapper around RawSmallStr/RawLargeStr
class RawStr : public RawObject {
 public:
  // Singletons.
  static RawStr empty();

  // Getters and setters.
  byte byteAt(word index) const;
  word length() const;
  void copyTo(byte* dst, word char_length) const;
  void copyToStartAt(byte* dst, word char_length, word char_start) const;

  // Equality checks.
  word compare(RawStr that) const;
  word compareCStr(const char* c_str) const;
  bool equals(RawStr that) const;
  bool equalsCStr(const char* c_str) const;

  bool includes(RawObject that) const;

  // Check for the presence of a non-zero byte.
  bool includesByte(byte b) const;

  // Codepoints
  int32_t codePointAt(word char_index, word* char_length) const;
  word codePointLength() const;
  bool isASCII() const;

  // Find the number of occurences of substring `needle`.
  word occurrencesOf(RawObject that) const;

  // Returns an index into a string offset by either a positive or negative
  // number of code points.  Otherwise, if the new index would be negative, -1
  // is returned or if the new index would be greater than the length of the
  // string, the length is returned.
  word offsetByCodePoints(word char_index, word count) const;

  // Conversion to an unescaped C string.  The underlying memory is allocated
  // with malloc and must be freed by the caller.
  char* toCStr() const;

  RAW_OBJECT_COMMON(Str);
};

// Immediate objects

class RawSmallInt : public RawObject {
 public:
  // Getters and setters.
  word value() const;
  void* asCPtr() const;
  // Converts a `SmallInt` created by `fromAlignedCPtr()` back to a pointer.
  void* asAlignedCPtr() const;
  word asReinterpretedWord() const;

  // If this fits in T, get its value as a T. If not, indicate what went wrong.
  template <typename T>
  if_signed_t<T, OptInt<T>> asInt() const;
  template <typename T>
  if_unsigned_t<T, OptInt<T>> asInt() const;

  word hash() const;

  // Construction.
  static RawSmallInt fromWord(word value);
  static RawSmallInt fromWordTruncated(word value);

  // Reinterpret a word value with the lowest `kSmallIntTagBits` cleared
  // directly as a `RawSmallInt` value, without performing the usual shift.
  static RawSmallInt fromReinterpretedWord(word value);

  // Create a `SmallInt` from an aligned C pointer.
  // This is slightly faster than `Runtime::newIntFromCPtr()` but only works for
  // pointers with an alignment of at least `2**kSmallIntTagBits`.
  // Use `toAlignedCPtr()` to reverse this operation; `toCPtr()` will not work
  // correctly.
  static RawSmallInt fromAlignedCPtr(void* ptr);
  static constexpr bool isValid(word value) {
    return (value >= kMinValue) && (value <= kMaxValue);
  }
  static word truncate(word value);

  // Constants.
  static const word kBits = kBitsPerPointer - kSmallIntTagBits;
  static const word kMinValue = -(word{1} << (kBits - 1));
  static const word kMaxValue = (word{1} << (kBits - 1)) - 1;

  static const word kMaxDigits10 = 18;
  static const word kMaxDigits10Pow = 1000000000000000000;
  static_assert((kMaxDigits10Pow <= kMaxValue) &&
                    (kMaxDigits10Pow > kMaxValue / 10),
                "invalid max");

  RAW_OBJECT_COMMON(SmallInt);
};

enum class ObjectFormat {
  // Instances that do not contain objects
  kData = 0,
  // Instances that contain objects
  kObjects = 1,
};

// RawHeader objects
//
// Headers are located in first logical word of a heap allocated object and
// contain metadata related to the object its part of.  A header is not
// really object that the user will interact with directly.  Nevertheless, we
// tag them as immediate object.  This allows the runtime to identify the start
// of an object when scanning the heap.
//
// Headers encode the following information
//
// Name      Size  Description
// ----------------------------------------------------------------------------
// Tag          3   tag for a header object
// Format       1   enumeration describing the object encoding
// LayoutId    20   identifier for the layout, allowing 2^20 unique layouts
// Count        8   number of array elements or instance variables
// Hash        32   bits to use for an identity hash code
class RawHeader : public RawObject {
 public:
  word hashCode() const;
  RawHeader withHashCode(word value) const;

  ObjectFormat format() const;

  LayoutId layoutId() const;
  RawHeader withLayoutId(LayoutId layout_id) const;

  word count() const;

  bool hasOverflow() const;

  static RawHeader from(word count, word hash, LayoutId id,
                        ObjectFormat format);

  // Layout.
  static const int kFormatBits = 1;
  static const int kFormatOffset = kPrimaryTagBits;
  static const uword kFormatMask = (1 << kFormatBits) - 1;

  static const int kLayoutIdBits = 20;
  static const int kLayoutIdOffset = kFormatOffset + kFormatBits;
  static const uword kLayoutIdMask = (1 << kLayoutIdBits) - 1;

  static const int kCountBits = 8;
  static const int kCountOffset = kLayoutIdOffset + kLayoutIdBits;
  static const uword kCountMask = (1 << kCountBits) - 1;

  static const int kHashCodeBits = 32;
  static const int kHashCodeOffset = kCountOffset + kCountBits;
  static const uword kHashCodeMask = (1UL << kHashCodeBits) - 1;

  static const int kTotalBits = kHashCodeOffset + kHashCodeBits;
  static_assert(kTotalBits == 64, "Header should be exactly 64 bits");

  static const int kCountOverflowFlag = (1 << kCountBits) - 1;
  static const int kCountMax = kCountOverflowFlag - 1;

  static const int kSize = kPointerSize;

  // Constants
  static const word kMaxLayoutId = (1L << (kLayoutIdBits + 1)) - 1;
  static const word kUninitializedHash = 0;

  RAW_OBJECT_COMMON(Header);
};

class RawSmallData : public RawObject {
 public:
  // Getters and setters.
  word length() const;
  byte byteAt(word index) const;
  void copyTo(byte* dst, word length) const;
  // Copy length bytes from this to dst, starting at the given index
  void copyToStartAt(byte* dst, word length, word index) const;
  bool isASCII() const;
  // Read adjacent bytes as `uint16_t` integer.
  uint16_t uint16At(word index) const;
  // Read adjacent bytes as `uint32_t` integer.
  uint32_t uint32At(word index) const;

  // Returns the index at which value is found in this[start:start+length] (not
  // including end), or -1 if not found.
  word findByte(byte value, word start, word length) const;

  // Check for the presence of a non-zero byte.
  bool includesByte(byte b) const;

  // Codepoints
  int32_t codePointAt(word char_index, word* char_length) const;
  word codePointLength() const;
  word offsetByCodePoints(word index, word count) const;

  // Conversion to an unescaped C string.  The underlying memory is allocated
  // with malloc and must be freed by the caller.
  char* toCStr() const;

  word hash() const;

  // Constants.
  static const word kMaxLength = kWordSize - 1;

  static const word kDataOffset = 1;

 protected:
  explicit RawSmallData(uword raw);
};

class RawSmallBytes : public RawSmallData {
 public:
  // Construction.
  static RawSmallBytes fromBytes(View<byte> data);
  static RawSmallBytes empty();

  // Rewrite the tag byte to make UTF-8 conformant bytes look like a Str
  RawObject becomeStr() const;

  RAW_OBJECT_COMMON(SmallBytes);

 private:
  explicit RawSmallBytes(uword raw);
};

class RawSmallStr : public RawSmallData {
 public:
  // Construction.
  static RawSmallStr fromCodePoint(int32_t code_point);
  static RawSmallStr fromCStr(const char* value);
  static RawSmallStr fromBytes(View<byte> data);
  static RawSmallStr empty();

  // Comparison
  word compare(RawObject that) const;
  word equalsCStr(const char* c_str) const;

  bool includes(RawObject that) const;

  // Check for the presence of a non-zero byte.
  bool includesByte(byte b) const;

  word occurrencesOf(RawObject that) const;

  RawObject becomeBytes() const;

  // Constants.
  static const word kMaxLength = kWordSize - 1;

  RAW_OBJECT_COMMON(SmallStr);

 private:
  explicit RawSmallStr(uword raw);
};

// An ErrorKind is in every RawError to give some high-level detail about what
// went wrong.
//
// Note that the only ErrorKind that implies a raised exception is Exception.
// All others are used either in situations where an exception wouldn't be
// appropriate, or where the error could be intercepted by runtime code before
// it has to be materialized into an actual exception, to avoid memory traffic
// on the Thread.
enum class ErrorKind : byte {
  // Generic RawError: when none of the other kinds fit. Should be avoided if
  // possible.
  kNone,

  // An exception was raised, and Thread::current()->hasPendingException() is
  // true.
  kException,

  // The attribute/function/dict entry/other named entity requested by the
  // caller was not found.
  kNotFound,

  // The given index was out of bounds for the container being searched.
  kOutOfBounds,

  // An allocation failed due to insufficient memory.
  kOutOfMemory,

  // An iterator hit the end of its container.
  kNoMoreItems,

  // If the largest ErrorKind is ever > 7, the immediate objects won't fit in
  // one byte, which may have performance implications.
};

// RawError is a special object type, internal to the runtime. It is used to
// signal that an error has occurred inside the runtime or native code, e.g. an
// exception has been raised or a value wasn't found during a lookup.
class RawError : public RawObject {
 public:
  // Singletons. See the documentation for ErrorKind for what each one means.
  static RawError error();
  static RawError exception();
  static RawError noMoreItems();
  static RawError notFound();
  static RawError outOfBounds();
  static RawError outOfMemory();

  // Kind.
  ErrorKind kind() const;

  // Bit Layout.
  static const int kTagMask = (1 << kBitsPerByte) - 1;
  static const int kKindOffset = kBitsPerByte;
  static const int kKindBits = 3;

  RAW_OBJECT_COMMON(Error);

 private:
  RawError(ErrorKind kind);
};

// Force users to call RawObject::isError*() rather than
// obj == Error::error(), since there isn't one unique RawError.
bool operator==(const RawError&, const RawObject&) = delete;
bool operator==(const RawObject&, const RawError&) = delete;
bool operator!=(const RawError&, const RawObject&) = delete;
bool operator!=(const RawObject&, const RawError&) = delete;

class RawBool : public RawObject {
 public:
  // Getters and setters.
  bool value() const;

  word hash() const;

  // Singletons
  static RawBool trueObj();
  static RawBool falseObj();

  // Construction.
  static RawBool fromBool(bool value);

  static RawBool negate(RawObject value);

  // Bit Layout.
  static const int kTagMask = (1 << kBitsPerByte) - 1;
  static const int kValueOffset = kBitsPerByte;

  RAW_OBJECT_COMMON(Bool);
};

class RawNotImplementedType : public RawObject {
 public:
  // Singletons.
  static RawNotImplementedType object();

  RAW_OBJECT_COMMON(NotImplementedType);
};

class RawUnbound : public RawObject {
 public:
  // Singletons.
  static RawUnbound object();

  RAW_OBJECT_COMMON(Unbound);
};

class RawNoneType : public RawObject {
 public:
  // Singletons.
  static RawNoneType object();

  RAW_OBJECT_COMMON(NoneType);
};

// Heap objects

class RawHeapObject : public RawObject {
 public:
  // Getters and setters.
  uword address() const;
  uword baseAddress() const;
  RawHeader header() const;
  void setHeader(RawHeader header) const;
  word headerOverflow() const;
  word headerCountOrOverflow() const;
  word size() const;

  // Construction.
  static RawHeapObject fromAddress(uword address);

  static RawHeapObject initializeHeader(uword address, word count, word hash,
                                        LayoutId id, ObjectFormat format);

  // Sizing
  static word headerSize(word count);

  // Garbage collection.
  bool isRoot() const;
  bool isForwarding() const;
  RawObject forward() const;
  void forwardTo(RawObject object) const;

  // Layout.
  static const int kHeaderOffset = -kPointerSize;
  static const int kHeaderOverflowOffset = kHeaderOffset - kPointerSize;
  static const int kSize = kHeaderOffset + kPointerSize;

  RAW_OBJECT_COMMON(HeapObject);
};

// Instances are objects with attributes described via the layout system.
class RawInstance : public RawHeapObject {
 public:
  // Sizing.
  static word allocationSize(word num_attr);

  // This is only public for the inline-cache to use. All other cases should
  // use more specific getter methods in the subclasses.
  RawObject instanceVariableAt(word offset) const;
  void instanceVariableAtPut(word offset, RawObject value) const;

  void setLayoutId(LayoutId layout_id) const;

  RAW_OBJECT_COMMON(Instance);

  // Instance initialization should only done by the Runtime.
  static RawObject initializeWithNone(uword address, word num_attributes,
                                      LayoutId layout_id);
  static RawObject initializeWithZero(uword address, word num_attributes,
                                      LayoutId layout_id);
};

class RawBaseException : public RawInstance {
 public:
  // Getters and setters.
  RawObject args() const;
  void setArgs(RawObject args) const;

  // The traceback, cause, and context can all be Unbound to indicate that they
  // are "NULL" rather than the normal unset value of None. The only code that
  // cares about the distinction is a handful of C-API functions, so the
  // standard getters transparently replace Unbound with None. The *OrUnbound
  // getters return the value as it's stored in memory, and are used
  // in the few C-API functions that care about the distinction.
  RawObject traceback() const;
  RawObject tracebackOrUnbound() const;
  void setTraceback(RawObject traceback) const;

  RawObject cause() const;
  RawObject causeOrUnbound() const;
  void setCause(RawObject cause) const;

  RawObject context() const;
  RawObject contextOrUnbound() const;
  void setContext(RawObject context) const;

  RawObject suppressContext() const;
  void setSuppressContext(RawObject suppress) const;

  static const int kArgsOffset = RawHeapObject::kSize;
  static const int kTracebackOffset = kArgsOffset + kPointerSize;
  static const int kCauseOffset = kTracebackOffset + kPointerSize;
  static const int kContextOffset = kCauseOffset + kPointerSize;
  static const int kSuppressContextOffset = kContextOffset + kPointerSize;
  static const int kSize = kSuppressContextOffset + kPointerSize;

  RAW_OBJECT_COMMON(BaseException);
};

class RawException : public RawBaseException {
 public:
  RAW_OBJECT_COMMON(Exception);
};

class RawSyntaxError : public RawException {
 public:
  static const int kFilenameOffset = RawException::kSize;
  static const int kLinenoOffset = kFilenameOffset + kPointerSize;
  static const int kMsgOffset = kLinenoOffset + kPointerSize;
  static const int kOffsetOffset = kMsgOffset + kPointerSize;
  static const int kPrintFileAndLineOffset = kOffsetOffset + kPointerSize;
  static const int kTextOffset = kPrintFileAndLineOffset + kPointerSize;
  static const int kSize = kTextOffset + kPointerSize;

  RAW_OBJECT_COMMON(SyntaxError);
};

class RawStopIteration : public RawBaseException {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  static const int kValueOffset = RawBaseException::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON(StopIteration);
};

class RawSystemExit : public RawBaseException {
 public:
  RawObject code() const;
  void setCode(RawObject code) const;

  static const int kCodeOffset = RawBaseException::kSize;
  static const int kSize = kCodeOffset + kPointerSize;

  RAW_OBJECT_COMMON(SystemExit);
};

class RawRuntimeError : public RawException {
 public:
  RAW_OBJECT_COMMON(RuntimeError);
};

class RawNotImplementedError : public RawRuntimeError {
 public:
  RAW_OBJECT_COMMON(NotImplementedError);
};

class RawImportError : public RawException {
 public:
  // Getters and setters
  RawObject msg() const;
  void setMsg(RawObject msg) const;

  RawObject name() const;
  void setName(RawObject name) const;

  RawObject path() const;
  void setPath(RawObject path) const;

  static const int kMsgOffset = RawBaseException::kSize;
  static const int kNameOffset = kMsgOffset + kPointerSize;
  static const int kPathOffset = kNameOffset + kPointerSize;
  static const int kSize = kPathOffset + kPointerSize;

  RAW_OBJECT_COMMON(ImportError);
};

class RawModuleNotFoundError : public RawImportError {
 public:
  RAW_OBJECT_COMMON(ModuleNotFoundError);
};

class RawLookupError : public RawException {
 public:
  RAW_OBJECT_COMMON(LookupError);
};

class RawIndexError : public RawLookupError {
 public:
  RAW_OBJECT_COMMON(IndexError);
};

class RawKeyError : public RawLookupError {
 public:
  RAW_OBJECT_COMMON(KeyError);
};

class RawUnicodeError : public RawException {
 public:
  RAW_OBJECT_COMMON(UnicodeError);
};

// This is a base class to allow for code reuse in the C++ implementations of
// the UnicodeError subclasses. According to the Python type system, each
// subclass of this Base class actually subclasses UnicodeError.
class RawUnicodeErrorBase : public RawException {
 public:
  RawObject encoding() const;
  void setEncoding(RawObject encoding_name) const;

  RawObject object() const;
  void setObject(RawObject value) const;

  RawObject start() const;
  void setStart(RawObject index) const;

  RawObject end() const;
  void setEnd(RawObject index) const;

  RawObject reason() const;
  void setReason(RawObject error_description) const;

  static const int kEncodingOffset = RawBaseException::kSize;
  static const int kObjectOffset = kEncodingOffset + kPointerSize;
  static const int kStartOffset = kObjectOffset + kPointerSize;
  static const int kEndOffset = kStartOffset + kPointerSize;
  static const int kReasonOffset = kEndOffset + kPointerSize;
  static const int kSize = kReasonOffset + kPointerSize;

  RAW_OBJECT_COMMON(UnicodeErrorBase);
};

class RawUnicodeDecodeError : public RawUnicodeErrorBase {
 public:
  RAW_OBJECT_COMMON(UnicodeDecodeError);
};

class RawUnicodeEncodeError : public RawUnicodeErrorBase {
 public:
  RAW_OBJECT_COMMON(UnicodeEncodeError);
};

class RawUnicodeTranslateError : public RawUnicodeErrorBase {
 public:
  RAW_OBJECT_COMMON(UnicodeTranslateError);
};

class RawAttributeDict : public RawInstance {
 public:
  RawObject attributes() const;
  void setAttributes(RawObject mutable_tuple) const;

  word attributesRemaining() const;
  void setAttributesRemaining(word free) const;

  static const int kAttributesOffset = RawInstance::kSize;
  static const int kAttributesRemainingOffset =
      kAttributesOffset + kPointerSize;
  static const int kSize = kAttributesRemainingOffset + kPointerSize;

  RAW_OBJECT_COMMON(AttributeDict);
};

class RawType : public RawAttributeDict {
 public:
  enum Flag : word {
    kNone = 0,

    // Bits 0-7 are reserved to hold a LayoutId.

    // Has non-empty __abstractmethods__
    kIsAbstract = 1 << 8,

    // The type has an attribute dictionary in cpython but is not using the
    // usual tuple-overflow or dict-overflow modes in the layout to provide it.
    kHasCustomDict = 1 << 9,

    // Instances have a block of of memory in the unmanaged C heap attached to
    // them. Instances are `RawNativeProxy`s.
    kHasNativeData = 1 << 10,

    // Has the extension flag Py_TPFLAGS_HAVE_GC
    kHasCycleGC = 1 << 11,

    // Has a default extension dealloc slot
    kHasDefaultDealloc = 1 << 12,

    // Has __slots__ in itself or its base
    kHasSlots = 1 << 13,

    // Runtime expects some attributes of this type to be at a fixed address.
    kIsFixedAttributeBase = 1 << 14,

    // Whether the type should behave like a CPython heap-type. Heap-types are
    // the default for user defined types. Non-heap types are used for most
    // built-in types. They have immutable type dictionaries and deal with
    // `__module__`, `__name__` and `__qualname__` in a different way.
    kIsCPythonHeaptype = 1 << 15,

    // Type may be used as a base class to create subclasses. A cleared basetype
    // is sometimes called "final class" in other languages.
    kIsBasetype = 1 << 16,

    // This type is an instance of a metaclass that defined an `mro` method and
    // potentially installed an irregular MRO.
    kHasCustomMro = 1 << 17,

    // this_type.__getattribute__ is object.__getattribute__.
    kHasObjectDunderGetattribute = 1 << 18,

    // this_type.__getattribute__ is type.__getattribute__.
    kHasTypeDunderGetattribute = 1 << 19,

    // this_type.__getattribute__ is module.__getattribute__.
    kHasModuleDunderGetattribute = 1 << 20,

    // this_type.__new__ is object.__new__.
    kHasObjectDunderNew = 1 << 21,

    // this_type.__hash__ is object.__hash__.
    kHasObjectDunderHash = 1 << 22,

    // this_type.__hash__ is str.__hash__.
    kHasStrDunderHash = 1 << 23,

    // this_type.__bool__ exists.
    kHasDunderBool = 1 << 24,

    // this_type.__len__ exists.
    kHasDunderLen = 1 << 25,

    // this_type.__class__ is object.__class__.
    kHasObjectDunderClass = 1 << 26,

    // this_type.__get__ exists.
    kHasDunderGet = 1 << 27,

    // this_type.__set__ exists.
    kHasDunderSet = 1 << 28,

    // this_type.__delete__ exists.
    kHasDunderDelete = 1 << 29,

    // this_type.__eq__ is object.__eq__.
    kHasObjectDunderEq = 1 << 30,
  };

  static const word kAttributeFlags =
      Flag::kHasObjectDunderGetattribute | Flag::kHasTypeDunderGetattribute |
      Flag::kHasModuleDunderGetattribute | Flag::kHasObjectDunderNew |
      Flag::kHasObjectDunderHash | Flag::kHasStrDunderHash |
      Flag::kHasDunderBool | Flag::kHasDunderLen | Flag::kHasObjectDunderClass |
      Flag::kHasDunderGet | Flag::kHasDunderSet | Flag::kHasDunderDelete |
      Flag::kHasObjectDunderEq;

  static const word kUninheritableFlags = Flag::kIsAbstract |
                                          Flag::kIsFixedAttributeBase |
                                          Flag::kIsBasetype | kAttributeFlags;

  static const word kInheritableFlags = ~kUninheritableFlags;

  // Getters and setters.
  RawObject instanceLayout() const;
  void setInstanceLayout(RawObject layout) const;

  LayoutId instanceLayoutId() const;
  void setInstanceLayoutId(LayoutId id) const;

  RawObject bases() const;
  void setBases(RawObject bases_tuple) const;

  RawObject doc() const;
  void setDoc(RawObject doc) const;

  RawObject mro() const;
  void setMro(RawObject object_array) const;

  RawObject name() const;
  void setName(RawObject name) const;

  // Flags.
  //
  // Bits 0-7 contain the LayoutId of the builtin base type. For builtin types,
  // this is the type itself, except for subtypes of int and str, which have
  // kInt and kStr, respectively. For user-defined types, it is the LayoutId of
  // the first builtin base class (kObject for most types).
  //
  // Bits 8+ are a bitmask of flags describing certain properties of the type.
  Flag flags() const;
  bool hasFlag(Flag bit) const;
  LayoutId builtinBase() const;
  void setFlags(Flag value) const;
  void setFlagsAndBuiltinBase(Flag value, LayoutId base) const;
  void setBuiltinBase(LayoutId base) const;

  bool isBuiltin() const;

  bool hasCustomDict() const;
  bool hasNativeData() const;
  bool isCPythonHeaptype() const;
  bool isBasetype() const;

  RawObject slots() const;
  void setSlots(RawObject slots) const;

  RawObject abstractMethods() const;
  void setAbstractMethods(RawObject methods) const;

  RawObject subclasses() const;
  void setSubclasses(RawObject subclasses) const;

  // Lazily allocated read-only proxy to the type dict.
  RawObject proxy() const;
  void setProxy(RawObject proxy) const;

  // Constructor function for this class.  Either type.__call__ or
  // a function that has the same effect as type.__call__.
  RawObject ctor() const;
  void setCtor(RawObject function) const;

  RawObject qualname() const;
  void setQualname(RawObject qualname) const;

  bool isBaseExceptionSubclass() const;

  // Check if the type dictionary is mutable. If the current type's dict is
  // immutable, its parents' dicts are immutable too.
  bool hasMutableDict() const;

  // Layout.
  static const int kMroOffset = RawAttributeDict::kSize;
  static const int kBasesOffset = kMroOffset + kPointerSize;
  static const int kInstanceLayoutOffset = kBasesOffset + kPointerSize;
  static const int kInstanceLayoutIdOffset =
      kInstanceLayoutOffset + kPointerSize;
  static const int kNameOffset = kInstanceLayoutIdOffset + kPointerSize;
  static const int kDocOffset = kNameOffset + kPointerSize;
  static const int kFlagsOffset = kDocOffset + kPointerSize;
  static const int kSlotsOffset = kFlagsOffset + kPointerSize;
  static const int kAbstractMethodsOffset = kSlotsOffset + kPointerSize;
  static const int kSubclassesOffset = kAbstractMethodsOffset + kPointerSize;
  static const int kProxyOffset = kSubclassesOffset + kPointerSize;
  static const int kCtorOffset = kProxyOffset + kPointerSize;
  static const int kQualnameOffset = kCtorOffset + kPointerSize;
  static const int kSize = kQualnameOffset + kPointerSize;

  static const int kBuiltinBaseMask = 0xff;

  RAW_OBJECT_COMMON(Type);
};

class RawContext : public RawInstance {
 public:
  // Getters and setters
  RawObject data() const;
  void setData(RawObject data) const;

  RawObject prevContext() const;
  void setPrevContext(RawObject prev_context) const;

  // Layout
  static const int kDataOffset = RawHeapObject::kSize;
  static const int kPrevContextOffset = kDataOffset + kPointerSize;
  static const int kSize = kPrevContextOffset + kPointerSize;

  RAW_OBJECT_COMMON(Context);
};

class RawContextVar : public RawInstance {
 public:
  // Getters and setters
  RawObject defaultValue() const;
  void setDefaultValue(RawObject default_value) const;

  RawObject name() const;
  void setName(RawObject name) const;

  // Layout
  static const int kDefaultValueOffset = RawHeapObject::kSize;
  static const int kNameOffset = kDefaultValueOffset + kPointerSize;
  static const int kSize = kNameOffset + kPointerSize;

  RAW_OBJECT_COMMON(ContextVar);
};

class RawTypeProxy : public RawInstance {
 public:
  // The type is instance is a proxy to.
  RawObject type() const;
  void setType(RawObject type) const;

  // Layout.
  static const int kTypeOffset = RawHeapObject::kSize;
  static const int kSize = kTypeOffset + kPointerSize;

  RAW_OBJECT_COMMON(TypeProxy);
};

class RawDataArray : public RawHeapObject {
 public:
  byte byteAt(word index) const;

  int32_t codePointAt(word char_index, word* char_length) const;
  word codePointLength() const;
  word offsetByCodePoints(word index, word count) const;

  void copyTo(byte* dst, word length) const;
  // Copy length bytes from this to dst, starting at the given index
  void copyToStartAt(byte* dst, word length, word index) const;

  word compare(RawDataArray that) const;
  bool equals(RawDataArray that) const;
  bool equalsBytes(View<byte> bytes) const;
  bool equalsCStr(const char* c_str) const;

  // Returns the index at which value is found in this[start:start+length] (not
  // including end), or -1 if not found.
  word findByte(byte value, word start, word length) const;

  // Check for the presence of a non-zero byte.
  bool includesByte(byte b) const;

  bool isASCII() const;

  word length() const;

  // Conversion to an unescaped C string.  The underlying memory is allocated
  // with malloc and must be freed by the caller.
  char* toCStr() const;

  // Read adjacent bytes as `uint16_t` integer.
  uint16_t uint16At(word index) const;
  // Read adjacent bytes as `uint32_t` integer.
  uint32_t uint32At(word index) const;
  // Read adjacent bytes as `uint64_t` integer.
  uint64_t uint64At(word index) const;

  // Sizing. Sizing and initialization should only be done by the Runtime.
  static word allocationSize(word length);
  static RawObject initialize(uword address, word length, LayoutId layout_id);

  RAW_OBJECT_COMMON(DataArray);
};

class RawLargeBytes : public RawDataArray {
 public:
  // Rewrite the header to make UTF-8 conformant bytes look like a Str
  RawObject becomeStr() const;

  RAW_OBJECT_COMMON(LargeBytes);

  // Sizing. Sizing should only be done by the Runtime.
  static word allocationSize(word length);
};

class RawLargeStr : public RawDataArray {
 public:
  bool includes(RawObject that) const;

  word occurrencesOf(RawObject that) const;

  // Sizing. Sizing should only be done by the Runtime.
  static word allocationSize(word length);

  RAW_OBJECT_COMMON(LargeStr);
};

class RawMutableBytes : public RawLargeBytes {
 public:
  void byteAtPut(word index, byte value) const;

  void uint16AtPut(word index, uint16_t value) const;

  void uint32AtPut(word index, uint32_t value) const;

  // Find the first occurrence from a specified start of any byte in the given
  // byte sequence, return the number of bytes read before the occurrence
  word indexOfAny(View<byte> needle, word start) const;

  // Replace the bytes from dst_start with count bytes from src
  void replaceFromWith(word dst_start, RawDataArray src, word count) const;

  // Replace the bytes from dst_start with count bytes from src, starting at
  // src_start in src
  void replaceFromWithStartAt(word dst_start, RawDataArray src, word count,
                              word src_start) const;

  // Replace the bytes from dst_start with count bytes from src
  void replaceFromWithBytes(word dst_start, RawBytes src, word count) const;

  void replaceFromWithByteslike(word dst_start, const Byteslike& byteslike,
                                word count) const;
  void replaceFromWithByteslikeStartAt(word dst_start,
                                       const Byteslike& byteslike, word count,
                                       word src_start) const;

  // Replace the bytes from dst_start with count bytes of value
  void replaceFromWithByte(word dst_start, byte value, word count) const;

  // Replace the bytes from dst_start with count bytes from src, starting at
  // src_start in src
  void replaceFromWithBytesStartAt(word dst_start, RawBytes src, word count,
                                   word src_start) const;

  // Replace the bytes from dst_start with count bytes from src.
  void replaceFromWithAll(word dst_start, View<byte> src) const;

  // Replace the bytes from index with len bytes from string src
  void replaceFromWithStr(word index, RawStr src, word char_length) const;

  void replaceFromWithStrStartAt(word dst_start, RawStr src, word char_length,
                                 word src_start_char) const;

  RawObject becomeImmutable() const;
  RawObject becomeStr() const;

  // Sizing. Sizing should only be done by the Runtime.
  static word allocationSize(word length);

  RAW_OBJECT_COMMON(MutableBytes);
};

// A mutable array, for the array module
//
// RawLayout:
//   [Header  ]
//   [Buffer  ] - Pointer to a RawMutableBytes with the underlying data.
//   [Length  ] - Number of bytes currently in the array.
//   [Typecode] - Typecode of the array
class RawArray : public RawInstance {
 public:
  // Getters and setters
  RawObject buffer() const;
  void setBuffer(RawObject new_buffer) const;

  word length() const;
  void setLength(word new_length) const;

  RawObject typecode() const;
  void setTypecode(RawObject new_typecode) const;

  // Layout
  static const int kBufferOffset = RawHeapObject::kSize;
  static const int kLengthOffset = kBufferOffset + kPointerSize;
  static const int kTypecodeOffset = kLengthOffset + kPointerSize;
  static const int kSize = kTypecodeOffset + kPointerSize;

  RAW_OBJECT_COMMON(Array);
};

// A container for an mmap'd pointer
//
// RawLayout:
//   [Header  ]
//   [Access  ] - A bitmask word storing the access permissions for the file
//   [Data    ] - A RawPointer holding the address + length of the memory
//   [Fd      ] - The file descriptor opened
class RawMmap : public RawInstance {
 public:
  enum Property {
    kReadable = 0x01,
    kWritable = 0x02,
    kCopyOnWrite = 0x04,
  };
  // Getters and setters
  word access() const;
  void setAccess(word new_access) const;

  RawObject data() const;
  void setData(RawObject new_data) const;

  RawObject fd() const;
  void setFd(RawObject new_fd) const;

  // Getters and setters for the kAccessOffset bitmask word
  bool isReadable() const;
  void setReadable() const;

  bool isWritable() const;
  void setWritable() const;

  bool isCopyOnWrite() const;
  void setCopyOnWrite() const;

  // Layout
  static const int kAccessOffset = RawHeapObject::kSize;
  static const int kDataOffset = kAccessOffset + kPointerSize;
  static const int kFdOffset = kDataOffset + kPointerSize;
  static const int kSize = kFdOffset + kPointerSize;

  RAW_OBJECT_COMMON(Mmap);
};

class RawTuple : public RawHeapObject {
 public:
  word length() const;

  // Getters and setters.
  RawObject at(word index) const;
  void atPut(word index, RawObject value) const;

  bool contains(RawObject object) const;

  RAW_OBJECT_COMMON(Tuple);
};

class RawMutableTuple : public RawTuple {
 public:
  // Sizing. Sizing should only be done by the Runtime.
  static word allocationSize(word length);

  // Finalizes this object and turns it into an immutable Tuple.
  RawObject becomeImmutable() const;

  void fill(RawObject value) const;

  // Copy count elements from src to this tuple, starting at index dst_start
  void replaceFromWith(word dst_start, RawTuple src, word count) const;

  // Copy count elements from src to this tuple, starting at index dst_start in
  // this and src_start in src
  void replaceFromWithStartAt(word dst_start, RawTuple src, word count,
                              word src_start) const;

  // Swap elements at indices i, j
  void swap(word i, word j) const;

  // Initialization should only be done by the Runtime.
  static RawObject initialize(uword address, word length);

  RAW_OBJECT_COMMON(MutableTuple);
};

class RawUserTupleBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserTupleBase);
};

RawTuple tupleUnderlying(RawObject object);

// Arbitrary precision signed integer, with 64 bit digits in two's complement
// representation
class RawLargeInt : public RawHeapObject {
 public:
  // Getters and setters.
  word asWord() const;

  // Return whether or not this RawLargeInt obeys the following invariants:
  // - numDigits() >= 1
  // - The value does not fit in a RawSmallInt
  // - Negative numbers do not have redundant sign-extended digits
  // - Positive numbers do not have redundant zero-extended digits
  bool isValid() const;

  // RawLargeInt is also used for storing native pointers.
  void* asCPtr() const;

  // If this fits in T, get its value as a T. If not, indicate what went wrong.
  template <typename T>
  if_signed_t<T, OptInt<T>> asInt() const;
  template <typename T>
  if_unsigned_t<T, OptInt<T>> asInt() const;

  // Indexing into digits
  uword digitAt(word index) const;
  void digitAtPut(word index, uword digit) const;

  bool isEven() const;
  bool isNegative() const;
  bool isPositive() const;

  word bitLength() const;

  // Number of digits
  word numDigits() const;

  // Copies digits bytewise to `dst`. Returns number of bytes copied.
  word copyTo(byte* dst, word copy_length) const;

  // Copy 'bytes' array into digits; if the array is too small set remaining
  // data to 'sign_extension' byte.
  void copyFrom(RawBytes bytes, byte sign_extension) const;

  // Layout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  // Sizing. Sizing and initialization should only be done by the Runtime.
  static word allocationSize(word num_digits);
  static RawObject initialize(uword address, word num_digits);

  RAW_OBJECT_COMMON(LargeInt);
};

class RawFloat : public RawHeapObject {
 public:
  // Getters and setters.
  double value() const;

  // Layout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kDoubleSize;

  // Instance initialization should only done by the Runtime.
  static RawObject initialize(uword address, double value);
  static word allocationSize();

  RAW_OBJECT_COMMON(Float);
};

class RawFrameProxy : public RawInstance {
 public:
  // The previous frame on the stack, or None if the current frame object
  // represents the bottom-most frame.
  RawObject back() const;
  void setBack(RawObject back) const;

  // The function executed on the frame.
  RawObject function() const;
  void setFunction(RawObject function) const;

  // The last instruction if called.
  RawObject lasti() const;
  void setLasti(RawObject lasti) const;

  // The local symbol table, a dictionary.
  RawObject locals() const;
  void setLocals(RawObject locals) const;

  // Layout.
  static const int kBackOffset = RawHeapObject::kSize;
  static const int kFunctionOffset = kBackOffset + kPointerSize;
  static const int kLastiOffset = kFunctionOffset + kPointerSize;
  static const int kLocalsOffset = kLastiOffset + kPointerSize;
  static const int kSize = kLocalsOffset + kPointerSize;

  RAW_OBJECT_COMMON(FrameProxy);
};

class RawUserBytesBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserBytesBase);
};

RawBytes bytesUnderlying(RawObject object);

class RawUserFloatBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserFloatBase);
};

RawFloat floatUnderlying(RawObject object);

class RawUserIntBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserIntBase);
};

RawInt intUnderlying(RawObject object);

class RawUserStrBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserStrBase);
};

RawStr strUnderlying(RawObject object);

class RawComplex : public RawHeapObject {
 public:
  // Getters
  double real() const;
  double imag() const;

  // Layout.
  static const int kRealOffset = RawHeapObject::kSize;
  static const int kImagOffset = kRealOffset + kDoubleSize;
  static const int kSize = kImagOffset + kDoubleSize;

  // Instance initialization should only done by the Runtime.
  static RawObject initialize(uword address, double real, double imag);
  static word allocationSize();

  RAW_OBJECT_COMMON(Complex);
};

class RawUserComplexBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserComplexBase);
};

RawComplex complexUnderlying(RawObject object);

class RawNativeProxy : public RawInstance {
 public:
  RawObject native() const;
  void setNative(RawObject native_ptr) const;

  RawObject dict() const;
  void setDict(RawObject dict) const;

  // A link to another object used by the garbage collector to create sets of
  // weak references for delayed processing.
  RawObject link() const;
  void setLink(RawObject reference) const;

  // TODO(eelizondo): Other finalizers will require the same logic. This should
  // be moved to a more generic location
  static void enqueue(RawObject reference, RawObject* tail);
  static RawObject dequeue(RawObject* tail);

  // Layout.NativeProxy appends its in-object attributes at the end of the given
  // base object.
  static const int kNativeOffsetFromEnd = -kPointerSize;
  static const int kDictOffsetFromEnd = kNativeOffsetFromEnd - kPointerSize;
  static const int kLinkOffsetFromEnd = kDictOffsetFromEnd - kPointerSize;
  static const int kSizeFromEnd = -kLinkOffsetFromEnd;

  RAW_OBJECT_COMMON_NO_CAST(NativeProxy);

 private:
  // Added here to prevent this field to be inherited from RawInstance.
  static const int kSize = -1;
};

class RawPointer : public RawHeapObject {
 public:
  // Getters and setters
  void* cptr() const;
  void setCPtr(void* new_cptr) const;

  word length() const;
  void setLength(word new_length) const;

  // Layout.
  static const int kCPtrOffset = RawHeapObject::kSize;
  static const int kLengthOffset = kCPtrOffset + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  // Instance initialization should only done by the Runtime.
  static RawObject initialize(uword address, void* cptr, word length);
  static word allocationSize();

  RAW_OBJECT_COMMON(Pointer);
};

class RawProperty : public RawInstance {
 public:
  // Getters and setters
  RawObject getter() const;
  void setGetter(RawObject function) const;

  RawObject setter() const;
  void setSetter(RawObject function) const;

  RawObject deleter() const;
  void setDeleter(RawObject function) const;

  // Layout.
  static const int kGetterOffset = RawHeapObject::kSize;
  static const int kSetterOffset = kGetterOffset + kPointerSize;
  static const int kDeleterOffset = kSetterOffset + kPointerSize;
  static const int kDocOffset = kDeleterOffset + kPointerSize;
  static const int kSize = kDocOffset + kPointerSize;

  RAW_OBJECT_COMMON(Property);
};

class RawRange : public RawInstance {
 public:
  // Getters and setters.
  RawObject start() const;
  void setStart(RawObject value) const;

  RawObject stop() const;
  void setStop(RawObject value) const;

  RawObject step() const;
  void setStep(RawObject value) const;

  // Layout.
  static const int kStartOffset = RawHeapObject::kSize;
  static const int kStopOffset = kStartOffset + kPointerSize;
  static const int kStepOffset = kStopOffset + kPointerSize;
  static const int kSize = kStepOffset + kPointerSize;

  RAW_OBJECT_COMMON(Range);
};

class RawSlice : public RawInstance {
 public:
  // Getters.
  RawObject start() const;
  RawObject stop() const;
  RawObject step() const;

  // Calculate the number of items that a slice addresses
  static word length(word start, word stop, word step);

  // Adjusts the slice indices to fit a collection with the given length.
  // Returns the length of the slice, and modifies start and stop to fit within
  // the bounds of the collection.
  //
  // If start or stop is negative, adjust them relative to length. If they are
  // still negative, sets them to zero. Limits start and stop to the length of
  // the collection if they are greater than the length.
  static word adjustIndices(word length, word* start, word* stop, word step);

  // Adjusts the bounds for searching a collection of the given length.
  //
  // NOTE: While this function is mostly the same as adjustIndices(), it does
  // not modify the start index when it is greater than the length.
  static void adjustSearchIndices(word* start, word* end, word length);

  // Layout.
  static const int kStartOffset = RawHeapObject::kSize;
  static const int kStopOffset = kStartOffset + kPointerSize;
  static const int kStepOffset = kStopOffset + kPointerSize;
  static const int kSize = kStepOffset + kPointerSize;

  RAW_OBJECT_COMMON(Slice);

 private:
  // Setters.
  void setStart(RawObject value) const;
  void setStep(RawObject value) const;
  void setStop(RawObject value) const;

  friend class Runtime;
};

class RawSlotDescriptor : public RawInstance {
 public:
  // Setters and getters.
  // Type that this descriptor is created for.
  RawObject type() const;
  void setType(RawObject type) const;

  // Name of attribute that this descriptor wraps.
  RawObject name() const;
  void setName(RawObject name) const;

  // Offset of the attribute this descriptor is for.
  word offset() const;
  void setOffset(word offset) const;

  // Layout.
  static const int kTypeOffset = RawHeapObject::kSize;
  static const int kNameOffset = kTypeOffset + kPointerSize;
  static const int kOffsetOffset = kNameOffset + kPointerSize;
  static const int kSize = kOffsetOffset + kPointerSize;

  RAW_OBJECT_COMMON(SlotDescriptor);
};

class RawStaticMethod : public RawInstance {
 public:
  // Getters and setters
  RawObject function() const;
  void setFunction(RawObject function) const;

  // Layout.
  static const int kFunctionOffset = RawHeapObject::kSize;
  static const int kSize = kFunctionOffset + kPointerSize;

  RAW_OBJECT_COMMON(StaticMethod);
};

class RawIteratorBase : public RawInstance {
 public:
  // Getters and setters.
  word index() const;
  void setIndex(word index) const;

  RawObject iterable() const;
  void setIterable(RawObject iterable) const;

  // Layout.
  static const int kIterableOffset = RawHeapObject::kSize;
  static const int kIndexOffset = kIterableOffset + kPointerSize;
  static const int kSize = kIndexOffset + kPointerSize;
};

class RawEnumerate : public RawInstance {
 public:
  RAW_OBJECT_COMMON(Enumerate);
  static const int kIteratorOffset = RawHeapObject::kSize;
  static const int kIndexOffset = kIteratorOffset + kPointerSize;
  static const int kSize = kIndexOffset + kPointerSize;
};

class RawBytearrayIterator : public RawIteratorBase {
 public:
  RAW_OBJECT_COMMON(BytearrayIterator);
};

class RawBytesIterator : public RawIteratorBase {
 public:
  RAW_OBJECT_COMMON(BytesIterator);
};

class RawDequeIterator : public RawIteratorBase {
 public:
  // Getters and setters.
  word state() const;
  void setState(word state) const;

  // Layout.
  static const int kStateOffset = RawIteratorBase::kSize;
  static const int kSize = kStateOffset + kPointerSize;

  RAW_OBJECT_COMMON(DequeIterator);
};

class RawDequeReverseIterator : public RawIteratorBase {
 public:
  // Getters and setters.
  word state() const;
  void setState(word state) const;

  // Layout.
  static const int kStateOffset = RawIteratorBase::kSize;
  static const int kSize = kStateOffset + kPointerSize;

  RAW_OBJECT_COMMON(DequeReverseIterator);
};

class RawDictIteratorBase : public RawIteratorBase {
 public:
  // Getters and setters.

  // This looks similar to index but is different and required in order to
  // implement interators properly. We cannot use index in __length_hint__
  // because index describes the position inside the internal buckets list of
  // our implementation of dict -- not the logical number of iterms. Therefore
  // we need an additional piece of state that refers to the logical number of
  // items seen so far.
  word numFound() const;
  void setNumFound(word num_found) const;

  // Layout
  static const int kNumFoundOffset = RawIteratorBase::kSize;
  static const int kSize = kNumFoundOffset + kPointerSize;
};

class RawDictItemIterator : public RawDictIteratorBase {
 public:
  RAW_OBJECT_COMMON(DictItemIterator);
};

class RawDictKeyIterator : public RawDictIteratorBase {
 public:
  RAW_OBJECT_COMMON(DictKeyIterator);
};

class RawDictValueIterator : public RawDictIteratorBase {
 public:
  RAW_OBJECT_COMMON(DictValueIterator);
};

class RawListIterator : public RawIteratorBase {
 public:
  RAW_OBJECT_COMMON(ListIterator);
};

class RawLongRangeIterator : public RawInstance {
 public:
  // Getters and setters.
  RawObject next() const;
  void setNext(RawObject next) const;

  RawObject stop() const;
  void setStop(RawObject stop) const;

  RawObject step() const;
  void setStep(RawObject step) const;

  // Layout.
  static const int kNextOffset = RawHeapObject::kSize;
  static const int kStopOffset = kNextOffset + kPointerSize;
  static const int kStepOffset = kStopOffset + kPointerSize;
  static const int kSize = kStepOffset + kPointerSize;

  RAW_OBJECT_COMMON(LongRangeIterator);
};

// RangeIterator guarantees that start, stop, step, and length are all SmallInt.
class RawRangeIterator : public RawInstance {
 public:
  // Getters and setters.
  word next() const;
  void setNext(word next) const;

  word step() const;
  void setStep(word step) const;

  word length() const;
  void setLength(word length) const;

  // Layout.
  static const int kNextOffset = RawHeapObject::kSize;
  static const int kStepOffset = kNextOffset + kPointerSize;
  static const int kLengthOffset = kStepOffset + kPointerSize;
  static const int kSize = kLengthOffset + kPointerSize;

  RAW_OBJECT_COMMON(RangeIterator);
};

class RawSeqIterator : public RawIteratorBase {
 public:
  RAW_OBJECT_COMMON(SeqIterator);
};

class RawSetIterator : public RawIteratorBase {
 public:
  // Getters and setters
  word consumedCount() const;
  void setConsumedCount(word consumed) const;

  // Layout.
  static const int kConsumedCountOffset = RawIteratorBase::kSize;
  static const int kSize = kConsumedCountOffset + kPointerSize;

  RAW_OBJECT_COMMON(SetIterator);
};

class RawStrIterator : public RawIteratorBase {
 public:
  RAW_OBJECT_COMMON(StrIterator);
};

class RawTupleIterator : public RawIteratorBase {
 public:
  // Getters and setters.
  word length() const;
  void setLength(word length) const;

  // Layout.
  static const int kLengthOffset = RawIteratorBase::kSize;
  static const int kSize = kLengthOffset + kPointerSize;

  RAW_OBJECT_COMMON(TupleIterator);
};

class RawCode : public RawInstance {
 public:
  // Matching CPython
  enum Flags {
    // Local variables are organized in an array and LOAD_FAST/STORE_FAST are
    // used when this flag is set. Otherwise local variable accesses use
    // LOAD_NAME/STORE_NAME to modify a dictionary ("implicit globals").
    kOptimized = 0x0001,
    // Local variables start in an uninitialized state. If this is not set then
    // the variables are initialized with the values in the implicit globals.
    kNewlocals = 0x0002,
    kVarargs = 0x0004,
    kVarkeyargs = 0x0008,
    kNested = 0x0010,
    kGenerator = 0x0020,
    kNofree = 0x0040,  // Shortcut for no free or cell vars
    kCoroutine = 0x0080,
    kIterableCoroutine = 0x0100,
    kAsyncGenerator = 0x0200,
    kFutureDivision = 0x20000,
    kFutureAbsoluteImport = 0x40000,
    kFutureWithStatement = 0x80000,
    kFuturePrintFunction = 0x100000,
    kFutureUnicodeLiterals = 0x200000,
    kFutureBarryAsBdfl = 0x400000,
    kFutureGeneratorStop = 0x800000,
    kFutureAnnotations = 0x1000000,
    kBuiltin = 0x2000000,
    kMetadata = 0x4000000,
    kLast = kMetadata,
  };

  // Getters and setters.
  word argcount() const;
  void setArgcount(word value) const;
  word totalArgs() const;

  // The number of positional only arguments.
  word posonlyargcount() const;
  void setPosonlyargcount(word value) const;

  RawObject cell2arg() const;
  void setCell2arg(RawObject value) const;

  RawObject cellvars() const;
  void setCellvars(RawObject value) const;
  word numCellvars() const;

  RawObject code() const;
  void setCode(RawObject value) const;

  RawObject consts() const;
  void setConsts(RawObject value) const;

  RawObject filename() const;
  void setFilename(RawObject value) const;

  word firstlineno() const;
  void setFirstlineno(word value) const;

  word flags() const;
  void setFlags(word value) const;

  RawObject freevars() const;
  void setFreevars(RawObject value) const;
  word numFreevars() const;

  bool isAsyncGenerator() const;
  bool isGeneratorLike() const;
  bool hasFreevarsOrCellvars() const;
  bool hasOptimizedAndNewlocals() const;
  bool hasOptimizedOrNewlocals() const;

  bool isNative() const;

  word kwonlyargcount() const;
  void setKwonlyargcount(word value) const;

  RawObject lnotab() const;
  void setLnotab(RawObject value) const;

  RawObject name() const;
  void setName(RawObject value) const;

  RawObject names() const;
  void setNames(RawObject value) const;

  word nlocals() const;
  void setNlocals(word value) const;

  // Converts the offset in this code's bytecode into the corresponding line
  // number in the backing source file.
  word offsetToLineNum(word offset) const;

  word stacksize() const;
  void setStacksize(word value) const;

  RawObject varnames() const;
  void setVarnames(RawObject value) const;

  // Returns nullptr if the function cannot be executed without a frame.
  void* intrinsic() const;
  void setIntrinsic(void* fp) const;

  // Layout.
  static const int kArgcountOffset = RawHeapObject::kSize;
  static const int kPosonlyargcountOffset = kArgcountOffset + kPointerSize;
  static const int kKwonlyargcountOffset =
      kPosonlyargcountOffset + kPointerSize;
  static const int kNlocalsOffset = kKwonlyargcountOffset + kPointerSize;
  static const int kStacksizeOffset = kNlocalsOffset + kPointerSize;
  static const int kFlagsOffset = kStacksizeOffset + kPointerSize;
  static const int kFirstlinenoOffset = kFlagsOffset + kPointerSize;
  static const int kCodeOffset = kFirstlinenoOffset + kPointerSize;
  static const int kConstsOffset = kCodeOffset + kPointerSize;
  static const int kNamesOffset = kConstsOffset + kPointerSize;
  static const int kVarnamesOffset = kNamesOffset + kPointerSize;
  static const int kFreevarsOffset = kVarnamesOffset + kPointerSize;
  static const int kCellvarsOffset = kFreevarsOffset + kPointerSize;
  static const int kCell2argOffset = kCellvarsOffset + kPointerSize;
  static const int kFilenameOffset = kCell2argOffset + kPointerSize;
  static const int kNameOffset = kFilenameOffset + kPointerSize;
  static const int kLnotabOffset = kNameOffset + kPointerSize;
  static const int kIntrinsicOffset = kLnotabOffset + kPointerSize;
  static const int kSize = kIntrinsicOffset + kPointerSize;

  static const word kCompileFlagsMask =
      Flags::kFutureDivision | Flags::kFutureAbsoluteImport |
      Flags::kFutureWithStatement | Flags::kFuturePrintFunction |
      Flags::kFutureUnicodeLiterals | Flags::kFutureBarryAsBdfl |
      Flags::kFutureGeneratorStop | Flags::kFutureAnnotations;

  RAW_OBJECT_COMMON(Code);
};

// A function object.
//
// This may contain a user-defined function or a built-in function.
//
// RawFunction objects have a set of pre-defined attributes, only some of which
// are writable outside of the runtime. The full set is defined at
//
//     https://docs.python.org/3/reference/datamodel.html
class RawFunction : public RawInstance {
 public:
  // An entry point into a function.
  //
  // The entry point is called with the current thread, the caller's stack
  // frame, and the number of arguments that have been pushed onto the stack.
  using Entry = RawObject (*)(Thread*, word);

  enum Flags {
    kNone = 0,
    // Matching Code::Flags (and CPython)
    kOptimized = RawCode::Flags::kOptimized,
    kNewlocals = RawCode::Flags::kNewlocals,
    kVarargs = RawCode::Flags::kVarargs,
    kVarkeyargs = RawCode::Flags::kVarkeyargs,
    kNested = RawCode::Flags::kNested,
    kGenerator = RawCode::Flags::kGenerator,
    kNofree = RawCode::Flags::kNofree,
    kCoroutine = RawCode::Flags::kCoroutine,
    kIterableCoroutine = RawCode::Flags::kIterableCoroutine,
    kAsyncGenerator = RawCode::Flags::kAsyncGenerator,
    kSimpleCall = RawCode::Flags::kLast << 1,   // Speeds detection of fast call
    kInterpreted = RawCode::Flags::kLast << 2,  // Executable by the interpreter
    kExtension = RawCode::Flags::kLast << 3,    // C-API extension function
    kCompiled = RawCode::Flags::kLast << 4,     // JIT-compiled
    kLast = kCompiled,
  };

  // Getters and setters.

  // A dict containing parameter annotations
  RawObject annotations() const;
  void setAnnotations(RawObject annotations) const;

  // The number of positional arguments.
  word argcount() const;
  void setArgcount(word value) const;

  // The code object backing this function or None
  RawObject code() const;
  void setCode(RawObject code) const;

  // A tuple of cell objects that contain bindings for the function's free
  // variables. Read-only to user code.
  RawObject closure() const;
  void setClosure(RawObject closure) const;

  // A tuple containing default values for arguments with defaults. Read-only
  // to user code.
  RawObject defaults() const;
  void setDefaults(RawObject defaults) const;
  bool hasDefaults() const;

  // The function's docstring
  RawObject doc() const;
  void setDoc(RawObject doc) const;

  // Returns the entry to be used when the function is invoked via
  // CALL_FUNCTION
  Entry entry() const;
  void setEntry(Entry thunk) const;

  // Returns the entry to be used when the function is invoked via
  // CALL_FUNCTION_KW
  Entry entryKw() const;
  void setEntryKw(Entry thunk) const;

  // Returns the entry to be used when the function is invoked via
  // CALL_FUNCTION_EX
  Entry entryEx() const;
  void setEntryEx(Entry thunk) const;

  // Returns the entry to be used when the function is invoked in assembly.
  void* entryAsm() const;
  void setEntryAsm(void* thunk) const;

  // Returns the function flags.
  word flags() const;
  void setFlags(word flags) const;

  // Returns true if the function is an async generator.
  bool isAsyncGenerator() const;

  // Returns true if the function is a coroutine.
  bool isCoroutine() const;

  // Returns true if function has `kExtension` flag set.
  bool isExtension() const;

  // Returns true if function has `kCompiled` flag set.
  bool isCompiled() const;

  // Returns true if the function is a coroutine, a generator, or an async
  // generator.
  bool isGeneratorLike() const;

  // Returns true if the function has free variables or cell variables.
  bool hasFreevarsOrCellvars() const;

  // Returns true if the function is a generator.
  bool isGenerator() const;

  // Returns true if the function is an iterable coroutine.
  bool isIterableCoroutine() const;

  // Returns true if the function has the optimized or newlocals flag.
  bool hasOptimizedOrNewlocals() const;

  // Returns true if the function has a simple calling convention.
  bool hasSimpleCall() const;

  // Returns true if the function has varargs.
  bool hasVarargs() const;

  // Returns true if the function has varargs or varkeyword arguments.
  bool hasVarargsOrVarkeyargs() const;

  // Returns true if the function has varkeyword arguments.
  bool hasVarkeyargs() const;

  // Returns true if the function consists of bytecode that can be executed
  // normally by the interpreter.
  bool isInterpreted() const;
  void setIsInterpreted(bool interpreted) const;

  // Returns nullptr if the function cannot be executed without a frame.
  void* intrinsic() const;
  void setIntrinsic(void* fp) const;

  // A dict containing defaults for keyword-only parameters
  RawObject kwDefaults() const;
  void setKwDefaults(RawObject kw_defaults) const;

  // The name of the module the function was defined in
  RawObject moduleName() const;
  void setModuleName(RawObject module_name) const;

  // The module where this function was defined
  RawObject moduleObject() const;
  void setModuleObject(RawObject module) const;

  // The function's name
  RawObject name() const;
  void setName(RawObject name) const;

  // The function's qualname
  RawObject qualname() const;
  void setQualname(RawObject qualname) const;

  // Maximum stack size used by the bytecode.
  RawObject stacksizeOrBuiltin() const;
  void setStacksizeOrBuiltin(RawObject stacksize_or_builtin) const;

  // Returns the number of parameters. This includes `code.argcount()`,
  // `code.kwonlyargcount()`, and an extra parameter for varargs and for
  // varkeyargs argument when necessary.
  word totalArgs() const;
  void setTotalArgs(word value) const;

  // Returns number of variables. This is the number of locals that are not
  // parameters plus the number of cell variables and free variables.
  word totalVars() const;
  void setTotalVars(word value) const;

  // Returns the number of locals. This is equivalent to
  // `code().nlocals() + code().numFreevars() + code().numCellvars()`.
  word totalLocals() const;

  // Bytecode rewritten to a variant that uses inline caching.
  RawObject rewrittenBytecode() const;
  void setRewrittenBytecode(RawObject rewritten_bytecode) const;

  // Tuple with values of the inline caches. See `ic.h`.
  RawObject caches() const;
  void setCaches(RawObject cache) const;

  // The function's dictionary
  RawObject dict() const;
  void setDict(RawObject dict) const;

  // Layout.
  static const int kCodeOffset = RawHeapObject::kSize;
  static const int kFlagsOffset = kCodeOffset + kPointerSize;
  static const int kArgcountOffset = kFlagsOffset + kPointerSize;
  static const int kTotalArgsOffset = kArgcountOffset + kPointerSize;
  static const int kTotalVarsOffset = kTotalArgsOffset + kPointerSize;
  static const int kStacksizeOrBuiltinOffset = kTotalVarsOffset + kPointerSize;
  static const int kDocOffset = kStacksizeOrBuiltinOffset + kPointerSize;
  static const int kNameOffset = kDocOffset + kPointerSize;
  static const int kQualnameOffset = kNameOffset + kPointerSize;
  static const int kModuleNameOffset = kQualnameOffset + kPointerSize;
  static const int kModuleObjectOffset = kModuleNameOffset + kPointerSize;
  static const int kDefaultsOffset = kModuleObjectOffset + kPointerSize;
  static const int kAnnotationsOffset = kDefaultsOffset + kPointerSize;
  static const int kKwDefaultsOffset = kAnnotationsOffset + kPointerSize;
  static const int kClosureOffset = kKwDefaultsOffset + kPointerSize;
  static const int kEntryOffset = kClosureOffset + kPointerSize;
  static const int kEntryKwOffset = kEntryOffset + kPointerSize;
  static const int kEntryExOffset = kEntryKwOffset + kPointerSize;
  static const int kEntryAsmOffset = kEntryExOffset + kPointerSize;
  static const int kRewrittenBytecodeOffset = kEntryAsmOffset + kPointerSize;
  static const int kCachesOffset = kRewrittenBytecodeOffset + kPointerSize;
  static const int kDictOffset = kCachesOffset + kPointerSize;
  static const int kIntrinsicOffset = kDictOffset + kPointerSize;
  static const int kSize = kIntrinsicOffset + kPointerSize;

  RAW_OBJECT_COMMON(Function);
};

class RawMappingProxy : public RawInstance {
 public:
  // Setters and getters.
  RawObject mapping() const;
  void setMapping(RawObject mapping) const;

  // Layout.
  static const int kMappingOffset = RawHeapObject::kSize;
  static const int kSize = kMappingOffset + kPointerSize;

  RAW_OBJECT_COMMON(MappingProxy);
};

// Descriptor for a block of memory.
// Contrary to cpython, this is a reference to a `bytes` object which may be
// moved around by the garbage collector.
class RawMemoryView : public RawInstance {
 public:
  // Setters and getters.
  RawObject buffer() const;
  void setBuffer(RawObject buffer) const;

  RawObject format() const;
  void setFormat(RawObject format) const;

  // Length in bytes.
  word length() const;
  void setLength(word length) const;

  // Original object that memoryview was created with
  RawObject object() const;
  void setObject(RawObject object) const;

  // An integer indicating how many dimensions of a multi-dimensional array the
  // memory represents.
  RawObject ndim() const;
  void setNdim(RawObject ndim) const;

  // Tuple of integers giving the shape of the memory as an N-dimensional array
  // In the 1-D case, shape will have one value which is equal to the length
  RawObject shape() const;
  void setShape(RawObject shape) const;

  // Private variable used to store the starting index of a memoryview.
  // Default value is 0
  word start() const;
  void setStart(word start) const;

  // Tuple of integers used to keep track of the number of bytes to step in each
  // dimension when traversing a memoryview buffer. In the 1-D case, strides
  // will will have one value which is equal to the step
  // Default value is (1,)
  RawObject strides() const;
  void setStrides(RawObject strides) const;

  bool readOnly() const;
  void setReadOnly(bool read_only) const;

  // Layout.
  static const int kBufferOffset = RawHeapObject::kSize;
  static const int kFormatOffset = kBufferOffset + kPointerSize;
  static const int kLengthOffset = kFormatOffset + kPointerSize;
  static const int kReadOnlyOffset = kLengthOffset + kPointerSize;
  static const int kObjectOffset = kReadOnlyOffset + kPointerSize;
  static const int kShapeOffset = kObjectOffset + kPointerSize;
  static const int kStartOffset = kShapeOffset + kPointerSize;
  static const int kStridesOffset = kStartOffset + kPointerSize;
  static const int kNdimOffset = kStridesOffset + kPointerSize;
  static const int kSize = kNdimOffset + kPointerSize;

  RAW_OBJECT_COMMON(MemoryView);
};

class RawModule : public RawAttributeDict {
 public:
  // Setters and getters.
  RawObject name() const;
  void setName(RawObject name) const;

  // Contains the numeric address of mode definition object for C-API modules or
  // zero if the module was not defined through the C-API.
  RawObject def() const;
  bool hasDef() const;
  void setDef(RawObject def) const;

  // Contains the numeric address of module state object for C-API modules or
  // zero if the module was not defined through the C-API.
  RawObject state() const;
  bool hasState() const;
  void setState(RawObject state) const;

  // Lazily allocated ModuleProxy instance that behaves like dict.
  RawObject moduleProxy() const;
  void setModuleProxy(RawObject module_proxy) const;

  // Unique ID allocated at module creation time.
  word id() const;
  void setId(word id) const;

  // Return true if the module is built-in; if so, the high bit of id is
  // tagged.
  bool isBuiltin() const;

  // Layout.
  static const int kNameOffset = RawAttributeDict::kSize;
  static const int kDefOffset = kNameOffset + kPointerSize;
  static const int kStateOffset = kDefOffset + kPointerSize;
  static const int kModuleProxyOffset = kStateOffset + kPointerSize;
  static const int kSize = kModuleProxyOffset + kPointerSize;

  static const uword kBuiltinTag = uword{1} << sizeof(uword);
  static const uword kBuiltinTagMask = ~kBuiltinTag;

  // Constants.
  static const word kMaxModuleId = RawHeader::kHashCodeMask;

  RAW_OBJECT_COMMON(Module);
};

class RawModuleProxy : public RawInstance {
 public:
  // Module that this ModuleProxy is created for.
  // moduleproxy.module().moduleproxy() == moduleproxy holds.
  RawObject module() const;
  void setModule(RawObject module) const;

  // Layout.
  static const int kModuleOffset = RawHeapObject::kSize;
  static const int kSize = kModuleOffset + kPointerSize;

  RAW_OBJECT_COMMON(ModuleProxy);
};

// A mutable array of bytes.
//
// Invariant: All allocated bytes past the end of the array are 0.
// Invariant: items() is a MutableBytes.
//
// RawLayout:
//   [Header  ]
//   [Items   ] - Pointer to a RawMutableBytes with the underlying data.
//   [NumItems] - Number of bytes currently in the array.
class RawBytearray : public RawInstance {
 public:
  // Getters and setters
  byte byteAt(word index) const;
  void byteAtPut(word index, byte value) const;
  void copyTo(byte* dst, word length) const;
  RawObject items() const;
  void setItems(RawObject new_items) const;
  word numItems() const;
  void setNumItems(word num_bytes) const;
  void downsize(word new_length) const;

  // The size of the underlying bytes
  word capacity() const;

  // Compares the bytes in this to the bytes in that. Returns a negative value
  // if this is less than that, positive if this is greater than that, and zero
  // if they have the same bytes. Does not guarantee to return -1, 0, or 1.
  word compare(RawBytes that, word that_len);

  // Replace the bytes from dst_start with count bytes from src
  void replaceFromWith(word dst_start, RawBytearray src, word count) const;

  // Replace the bytes from dst_start with count bytes from src, starting at
  // src_start in src
  void replaceFromWithStartAt(word dst_start, RawBytearray src, word count,
                              word src_start) const;

  // Layout
  static const int kItemsOffset = RawHeapObject::kSize;
  static const int kNumItemsOffset = kItemsOffset + kPointerSize;
  static const int kSize = kNumItemsOffset + kPointerSize;

  RAW_OBJECT_COMMON(Bytearray);
};

// A mutable Unicode array, for internal string building.
//
// Invariant: The allocated code units form valid UTF-8.
//
// RawLayout:
//   [Header  ]
//   [Items   ] - Pointer to a RawMutableBytes with the underlying data.
//   [NumItems] - Number of bytes currently in the array.
class RawStrArray : public RawInstance {
 public:
  // Getters and setters
  RawObject items() const;
  void setItems(RawObject new_items) const;
  word numItems() const;
  void setNumItems(word num_items) const;

  void copyTo(byte* dst, word length) const;
  int32_t codePointAt(word index, word* length) const;

  // Returns an index into a string offset by either a positive or negative
  // number of code points.  Otherwise, if the new index would be negative, -1
  // is returned or if the new index would be greater than the length of the
  // string, the length is returned.
  word offsetByCodePoints(word char_index, word count) const;

  // Rotate the code point from `last` to `first`.
  void rotateCodePoint(word first, word last) const;

  // The size of the underlying string in bytes.
  word capacity() const;

  // Layout
  static const int kItemsOffset = RawHeapObject::kSize;
  static const int kNumItemsOffset = kItemsOffset + kPointerSize;
  static const int kSize = kNumItemsOffset + kPointerSize;

  RAW_OBJECT_COMMON(StrArray);
};

// A double-ended queue
//
// RawLayout:
//   [Header  ]
//   [Items   ] - data
//   [Left    ] - head element
//   [NumItems] - number of elements
//   [Maxlen  ] - maximum capacity
class RawDeque : public RawInstance {
 public:
  RawObject at(word index) const;
  void atPut(word index, RawObject value) const;

  // Returns the total number of elements that may be held without growing
  // the underlying MutableTuple
  word capacity() const;

  void clear() const;

  // Getters and Setters
  RawObject items() const;
  void setItems(RawObject new_items) const;

  word left() const;
  void setLeft(word left) const;

  word numItems() const;
  void setNumItems(word num_items) const;

  RawObject maxlen() const;
  void setMaxlen(RawObject maxlen) const;

  word state() const;
  void setState(word state) const;

  // Layout.
  static const int kItemsOffset = RawHeapObject::kSize;
  static const int kLeftOffset = kItemsOffset + kPointerSize;
  static const int kNumItemsOffset = kLeftOffset + kPointerSize;
  static const int kMaxlenOffset = kNumItemsOffset + kPointerSize;
  static const int kStateOffset = kMaxlenOffset + kPointerSize;
  static const int kSize = kStateOffset + kPointerSize;

  RAW_OBJECT_COMMON(Deque);
};

// A simple dict that uses open addressing and linear probing.
//
// RawLayout:
//
//   [Header  ]
//   [NumItems] - Number of items currently in the dict
//   [Data    ] - RawTuple that stores the underlying data.
//   [Indices ] - RawTuple storing indices into the data tuple.
//   [FirstEmptyItemIndex] - Index pointing to the first empty item in data.
//
class RawDict : public RawInstance {
 public:
  // Number of items currently in the dict
  word numItems() const;
  void setNumItems(word num_items) const;

  // Getters and setters.
  // RawTuple that stores the underlying data.
  RawObject data() const;
  void setData(RawObject data) const;

  // RawTuple storing indices into the data tuple.
  RawObject indices() const;
  void setIndices(RawObject index_data) const;

  // Index pointing to the first empty item in data.
  word firstEmptyItemIndex() const;
  void setFirstEmptyItemIndex(word first_empty_item_index) const;

  // Number of indices.
  word numIndices() const;

  // Layout.
  static const int kNumItemsOffset = RawHeapObject::kSize;
  static const int kDataOffset = kNumItemsOffset + kPointerSize;
  static const int kIndicesOffset = kDataOffset + kPointerSize;
  static const int kFirstEmptyItemIndexOffset = kIndicesOffset + kPointerSize;
  static const int kSize = kFirstEmptyItemIndexOffset + kPointerSize;

  RAW_OBJECT_COMMON(Dict);
};

class RawDictViewBase : public RawInstance {
 public:
  // Getters and setters
  RawObject dict() const;
  void setDict(RawObject dict) const;

  // Layout
  static const int kDictOffset = RawHeapObject::kSize;
  static const int kSize = kDictOffset + kPointerSize;
};

class RawDictItems : public RawDictViewBase {
 public:
  RAW_OBJECT_COMMON(DictItems);
};

class RawDictKeys : public RawDictViewBase {
 public:
  RAW_OBJECT_COMMON(DictKeys);
};

class RawDictValues : public RawDictViewBase {
 public:
  RAW_OBJECT_COMMON(DictValues);
};

// A simple set implementation. Used by set and frozenset.
class RawSetBase : public RawInstance {
 public:
  // Getters and setters.
  // The RawTuple backing the set
  RawObject data() const;
  void setData(RawObject data) const;

  // Number of items currently in the set
  word numItems() const;
  void setNumItems(word num_items) const;

  // Number of active and tombstone items in the set
  word numFilled() const;
  void setNumFilled(word num_filled) const;

  // Layout.
  static const int kDataOffset = RawHeapObject::kSize;
  static const int kNumItemsOffset = kDataOffset + kPointerSize;
  static const int kNumFilledOffset = kNumItemsOffset + kPointerSize;
  static const int kSize = kNumFilledOffset + kPointerSize;

  RAW_OBJECT_COMMON(SetBase);
};

class RawSet : public RawSetBase {
 public:
  RAW_OBJECT_COMMON(Set);
};

class RawFrozenSet : public RawSetBase {
 public:
  RAW_OBJECT_COMMON(FrozenSet);
};

// A growable array
//
// RawLayout:
//
//   [Header  ]
//   [Items   ] - Pointer to an RawTuple that contains list elements
//   [NumItems] - Number of elements currently in the list
class RawList : public RawInstance {
 public:
  // Getters and setters.
  RawObject at(word index) const;
  void atPut(word index, RawObject value) const;
  RawObject items() const;
  void setItems(RawObject new_items) const;
  word numItems() const;
  void setNumItems(word num_items) const;
  void clearFrom(word idx) const;

  // Return the total number of elements that may be held without growing the
  // list
  word capacity() const;

  // Copy count elements from src to this list, starting at index start
  void replaceFromWith(word start, RawList src, word count) const;

  // Copy count elements from src to this list, starting at index start in the
  // destination and index src_start in the source
  void replaceFromWithStartAt(word start, RawList src, word count,
                              word src_start) const;

  // Swap elements at indices i, j
  void swap(word i, word j) const;

  // Layout.
  static const int kItemsOffset = RawHeapObject::kSize;
  static const int kNumItemsOffset = kItemsOffset + kPointerSize;
  static const int kSize = kNumItemsOffset + kPointerSize;

  RAW_OBJECT_COMMON(List);
};

class RawValueCell : public RawInstance {
 public:
  // Getters and setters
  RawObject value() const;
  void setValue(RawObject object) const;
  RawObject dependencyLink() const;
  void setDependencyLink(RawObject object) const;
  bool isPlaceholder() const;
  void makePlaceholder() const;

  // Layout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kDependencyLinkOffset = kValueOffset + kPointerSize;
  static const int kSize = kDependencyLinkOffset + kPointerSize;

  RAW_OBJECT_COMMON(ValueCell);
};

class RawEllipsis : public RawHeapObject {
 public:
  static word allocationSize();

  // Initialization should only be done by the Runtime.
  static RawObject initialize(uword address);

  RAW_OBJECT_COMMON(Ellipsis);
};

class RawToken : public RawInstance {
 public:
  // Getters and setters
  RawObject context() const;
  void setContext(RawObject context) const;

  RawObject oldValue() const;
  void setOldValue(RawObject old_value) const;

  bool used() const;
  void setUsed(bool used) const;

  RawObject var() const;
  void setVar(RawObject var) const;

  // Layout
  static const int kContextOffset = RawHeapObject::kSize;
  static const int kOldValueOffset = kContextOffset + kPointerSize;
  static const int kUsedOffset = kOldValueOffset + kPointerSize;
  static const int kVarOffset = kUsedOffset + kPointerSize;
  static const int kSize = kVarOffset + kPointerSize;

  RAW_OBJECT_COMMON(Token);
};

class RawWeakRef : public RawInstance {
 public:
  // Getters and setters

  // The object weakly-referenced by this instance.  Set to None by the garbage
  // collector when the referent is being collected.
  RawObject referent() const;
  void setReferent(RawObject referent) const;

  // A callable object invoked with the weakref object as an argument when the
  // referent is deemed to be "near death" and only reachable through a weak
  // reference.
  RawObject callback() const;
  void setCallback(RawObject callable) const;

  // A link to another object used by the garbage collector to create sets of
  // weak references for delayed processing.
  RawObject link() const;
  void setLink(RawObject reference) const;

  // The referent's hash
  RawObject hash() const;
  void setHash(RawObject hash) const;

  static void enqueue(RawObject reference, RawObject* tail);
  static RawObject dequeue(RawObject* tail);
  static RawObject spliceQueue(RawObject tail1, RawObject tail2);

  // Layout.
  static const int kReferentOffset = RawHeapObject::kSize;
  static const int kCallbackOffset = kReferentOffset + kPointerSize;
  static const int kLinkOffset = kCallbackOffset + kPointerSize;
  static const int kHashOffset = kLinkOffset + kPointerSize;
  static const int kSize = kHashOffset + kPointerSize;

  RAW_OBJECT_COMMON(WeakRef);
};

class RawUserWeakRefBase : public RawInstance {
 public:
  // Getters and setters.
  RawObject value() const;
  void setValue(RawObject value) const;

  // RawLayout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UserWeakRefBase);
};

RawWeakRef weakRefUnderlying(RawObject object);

class RawWeakProxy : public RawInstance {
 public:
  // Getters and setters

  // The object weakly-referenced by this instance.  Set to None by the garbage
  // collector when the referent is being collected.
  RawObject referent() const;
  void setReferent(RawObject referent) const;

  // Layout.
  static const int kReferentOffset = RawHeapObject::kSize;
  static const int kSize = kReferentOffset + kPointerSize;

  RAW_OBJECT_COMMON(WeakProxy);
};

class RawWeakCallableProxy : public RawInstance {
 public:
  // Getters and setters

  // The object weakly-referenced by this instance.  Set to None by the garbage
  // collector when the referent is being collected.
  RawObject referent() const;
  void setReferent(RawObject referent) const;

  // Layout.
  static const int kReferentOffset = RawHeapObject::kSize;
  static const int kSize = kReferentOffset + kPointerSize;

  RAW_OBJECT_COMMON(WeakCallableProxy);
};

// RawWeakLink objects are used to form double linked lists where the elements
// can still be garbage collected.
//
// A main usage of this is to maintain a list of function objects
// to be notified of global variable cache invalidation.
class RawWeakLink : public RawWeakRef {
 public:
  // Getters and setters.
  RawObject next() const;
  void setNext(RawObject object) const;
  RawObject prev() const;
  void setPrev(RawObject object) const;

  // Layout.
  static const int kNextOffset = RawWeakRef::kSize;
  static const int kPrevOffset = kNextOffset + kPointerSize;
  static const int kSize = kPrevOffset + kPointerSize;

  RAW_OBJECT_COMMON(WeakLink);
};

// A RawBoundMethod binds a RawFunction and its first argument (called `self`).
//
// These are typically created as a temporary object during a method call,
// though they may be created and passed around freely.
//
// Consider the following snippet of python code:
//
//   class Foo:
//     def bar(self):
//       return self
//   f = Foo()
//   f.bar()
//
// The Python 3.6 bytecode produced for the line `f.bar()` is:
//
//   LOAD_FAST                0 (f)
//   LOAD_ATTR                1 (bar)
//   CALL_FUNCTION            0
//
// The LOAD_ATTR for `f.bar` creates a `RawBoundMethod`, which is then called
// directly by the subsequent CALL_FUNCTION opcode.
class RawBoundMethod : public RawInstance {
 public:
  // Getters and setters

  // The function to which "self" is bound
  RawObject function() const;
  void setFunction(RawObject function) const;

  // The instance of "self" being bound
  RawObject self() const;
  void setSelf(RawObject self) const;

  // Layout.
  static const int kFunctionOffset = RawHeapObject::kSize;
  static const int kSelfOffset = kFunctionOffset + kPointerSize;
  static const int kSize = kSelfOffset + kPointerSize;

  RAW_OBJECT_COMMON(BoundMethod);
};

class RawCell : public RawInstance {
 public:
  // Getters and setters
  RawObject value() const;
  void setValue(RawObject value) const;

  // Layout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON(Cell);
};

class RawClassMethod : public RawInstance {
 public:
  // Getters and setters
  RawObject function() const;
  void setFunction(RawObject function) const;

  // Layout.
  static const int kFunctionOffset = RawHeapObject::kSize;
  static const int kSize = kFunctionOffset + kPointerSize;

  RAW_OBJECT_COMMON(ClassMethod);
};

// A RawLayout describes the in-memory shape of an instance.
//
// RawInstance attributes are split into two classes: in-object attributes,
// which exist directly in the instance, and overflow attributes, which are
// stored in an object array pointed to by the last word of the instance.
// Graphically, this looks like:
//
//   RawInstance                                   RawTuple
//   +---------------------------+     +------->+--------------------------+
//   | First in-object attribute |     |        | First overflow attribute |
//   +---------------------------+     |        +--------------------------+
//   |            ...            |     |        |           ...            |
//   +---------------------------+     |        +--------------------------+
//   | Last in-object attribute  |     |        | Last overflow attribute  |
//   +---------------------------+     |        +--------------------------+
//   | Overflow Attributes       +-----+
//   +---------------------------+
//
// Each instance is associated with a layout (whose id is stored in the header
// word). The layout acts as a roadmap for the instance; it describes where to
// find each attribute.
//
// In general, instances of the same class will have the same shape. Idiomatic
// Python typically initializes attributes in the same order for instances of
// the same class. Ideally, we would be able to share the same concrete
// RawLayout between two instances of the same shape. This both reduces memory
// overhead and enables effective caching of attribute location.
//
// To achieve structural sharing, layouts form an immutable DAG. Every class
// has a root layout that contains only in-object attributes. When an instance
// is created, it is assigned the root layout of its class. When a shape
// altering mutation to the instance occurs (e.g. adding an attribute), the
// current layout is searched for a corresponding edge. If such an edge exists,
// it is followed and the instance is assigned the resulting layout. If there
// is no such edge, a new layout is created, an edge is inserted between
// the two layouts, and the instance is assigned the new layout.
class RawLayout : public RawInstance {
 public:
  // Getters and setters.
  LayoutId id() const;
  void setId(LayoutId id) const;

  // Returns the class whose instances are described by this layout
  RawObject describedType() const;
  void setDescribedType(RawObject type) const;

  // Set the number of in-object attributes that may be stored on an instance
  // described by this layout.
  //
  // N.B. - This will always be larger than or equal to the length of the
  // RawTuple returned by inObjectAttributes().
  void setNumInObjectAttributes(word count) const;
  word numInObjectAttributes() const;

  // Returns an RawTuple describing the attributes stored directly in
  // in the instance.
  //
  // Each item in the object array is a two element tuple. Each tuple is
  // composed of the following elements, in order:
  //
  //   1. The attribute name (RawStr, or NoneType if unnamed (name is kInvalid))
  //   2. The attribute info (AttributeInfo)
  RawObject inObjectAttributes() const;
  void setInObjectAttributes(RawObject attributes) const;

  // Returns an RawTuple describing the attributes stored in the overflow
  // array of the instance.
  //
  // Each item in the object array is a two element tuple. Each tuple is
  // composed of the following elements, in order:
  //
  //   1. The attribute name (RawStr)
  //   2. The attribute info (AttributeInfo)
  RawObject overflowAttributes() const;
  void setOverflowAttributes(RawObject attributes) const;

  void setDictOverflowOffset(word offset) const;
  word dictOverflowOffset() const;

  // Returns a flattened list of tuples. Each tuple is composed of the
  // following elements, in order:
  //
  //   1. The attribute name (RawStr)
  //   2. The layout that would result if an attribute with that name
  //      was added.
  RawObject additions() const;
  void setAdditions(RawObject additions) const;

  // Returns a flattened list of tuples. Each tuple is composed of the
  // following elements, in order:
  //
  //   1. The attribute name (RawStr)
  //   2. The layout that would result if an attribute with that name
  //      was deleted.
  RawObject deletions() const;
  void setDeletions(RawObject deletions) const;

  // Returns the number of bytes in an instance described by this layout,
  // including the overflow array. Computed from the number of in-object
  // attributes and possible overflow slot.
  word instanceSize() const;

  // Return the offset, in bytes, of the overflow slot
  word overflowOffset() const;

  // Seal the attributes of the layout.
  void seal() const;

  // Returns true if the layout has sealed attributes.
  bool isSealed() const;

  // Returns true if the layout is for a NativeProxy type.
  bool isNativeProxyLayout() const;

  // Returns true if the layout stores its overflow attributes in a dictionary.
  bool hasDictOverflow() const;

  // Returns true if the layout stores its overflow attributes in a tuple.
  bool hasTupleOverflow() const;

  // Layout.
  static const int kDescribedTypeOffset = RawHeapObject::kSize;
  static const int kInObjectAttributesOffset =
      kDescribedTypeOffset + kPointerSize;
  static const int kOverflowAttributesOffset =
      kInObjectAttributesOffset + kPointerSize;
  static const int kAdditionsOffset = kOverflowAttributesOffset + kPointerSize;
  static const int kDeletionsOffset = kAdditionsOffset + kPointerSize;
  static const int kNumInObjectAttributesOffset =
      kDeletionsOffset + kPointerSize;
  static const int kSize = kNumInObjectAttributesOffset + kPointerSize;

  RAW_OBJECT_COMMON(Layout);
};

class RawSuper : public RawInstance {
 public:
  // getters and setters
  RawObject type() const;
  void setType(RawObject type) const;
  RawObject object() const;
  void setObject(RawObject obj) const;
  RawObject objectType() const;
  void setObjectType(RawObject type) const;

  // Layout.
  static const int kTypeOffset = RawHeapObject::kSize;
  static const int kObjectOffset = kTypeOffset + kPointerSize;
  static const int kObjectTypeOffset = kObjectOffset + kPointerSize;
  static const int kSize = kObjectTypeOffset + kPointerSize;

  RAW_OBJECT_COMMON(Super);
};

// TODO(T63568836): Replace GeneratorFrame by moving it variable
// length data into a MutableTuple and moving any other attributes
// into Generator.
// A Frame in a HeapObject, with space allocated before and after for stack and
// locals, respectively. It looks almost exactly like the ascii art diagram for
// Frame (from frame.h), except that there is a fixed amount of space allocated
// for the value stack, which comes from stacksize() on the Code object this is
// created from:
//
//   +----------------------+  <--+
//   | Arg 0                |     |
//   | ...                  |     |
//   | Arg N                |     |
//   | Local 0              |     | (totalArgs() + totalVars()) * kPointerSize
//   | ...                  |     |
//   | Local N              |     |
//   +----------------------+  <--+
//   |                      |     |
//   | Frame                |     | Frame::kSize
//   |                      |     |
//   +----------------------+  <--+  <-- frame()
//   |                      |     |
//   | Value stack          |     | maxStackSize() * kPointerSize
//   |                      |     |
//   +----------------------+  <--+
//   | maxStackSize         |
//   +----------------------+
class RawGeneratorFrame : public RawInstance {
 public:
  // The size of the embedded frame + stack and locals, in words.
  word numFrameWords() const;

  // Get or set the number of words allocated for the value stack. Used to
  // derive a pointer to the Frame inside this GeneratorFrame.
  word maxStackSize() const;
  void setMaxStackSize(word offset) const;

  // Returns the function of a heap frame.
  // Note that using `frame()->function()` does not work for this!
  RawObject function() const;

  // Accessors to the contained frame.
  RawObject popValue() const;
  void setVirtualPC(word value) const;
  RawObject* valueStackTop() const;
  word virtualPC() const;

  word stackSize() const;
  void setStackSize(word size) const;

  // Sizing.
  static word numAttributes(word extra_words);

  // Layout.
  static const int kMaxStackSizeOffset = RawHeapObject::kSize;
  static const int kFrameOffset = kMaxStackSizeOffset + kPointerSize;

  // Size and offsets within frame. Keep in sync with `Frame` class!
  static const int kFrameSize = 26 * kPointerSize;
  static const int kStackSizeFrameOffset = 2 * kPointerSize;

  // Number of words that aren't the Frame.
  static const int kNumOverheadWords = kFrameOffset / kPointerSize;

  RAW_OBJECT_COMMON(GeneratorFrame);

 private:
  // The Frame contained in this GeneratorFrame.
  Frame* frame() const;
};

// The exception currently being handled. Every Generator and Coroutine has its
// own exception state that is installed while it's running, to allow yielding
// from an except block without losing track of the caught exception.
//
// TODO(T38009294): This class won't exist forever. Think very hard about
// adding any more bits of state to it.
class RawExceptionState : public RawInstance {
 public:
  // Getters and setters.
  RawObject type() const;
  RawObject value() const;
  RawObject traceback() const;

  void setType(RawObject type) const;
  void setValue(RawObject value) const;
  void setTraceback(RawObject tb) const;

  RawObject previous() const;
  void setPrevious(RawObject prev) const;

  // Layout.
  static const int kTypeOffset = RawHeapObject::kSize;
  static const int kValueOffset = kTypeOffset + kPointerSize;
  static const int kTracebackOffset = kValueOffset + kPointerSize;
  static const int kPreviousOffset = kTracebackOffset + kPointerSize;
  static const int kSize = kPreviousOffset + kPointerSize;

  RAW_OBJECT_COMMON(ExceptionState);
};

// Base class containing functionality needed by all objects representing a
// suspended execution frame: RawGenerator, RawCoroutine, and AsyncGenerator.
class RawGeneratorBase : public RawInstance {
 public:
  // Get or set the RawGeneratorFrame embedded in this RawGeneratorBase.
  RawObject generatorFrame() const;
  void setGeneratorFrame(RawObject obj) const;

  RawObject exceptionState() const;
  void setExceptionState(RawObject obj) const;

  RawObject name() const;
  void setName(RawObject obj) const;

  RawObject running() const;
  void setRunning(RawObject obj) const;

  RawObject qualname() const;
  void setQualname(RawObject obj) const;

  // Layout.
  static const int kFrameOffset = RawHeapObject::kSize;
  static const int kExceptionStateOffset = kFrameOffset + kPointerSize;
  static const int kNameOffset = kExceptionStateOffset + kPointerSize;
  static const int kQualnameOffset = kNameOffset + kPointerSize;
  static const int kRunningOffset = kQualnameOffset + kPointerSize;
  static const int kSize = kRunningOffset + kPointerSize;

  RAW_OBJECT_COMMON(GeneratorBase);
};

class RawGenerator : public RawGeneratorBase {
 public:
  static const int kYieldFromOffset = RawGeneratorBase::kSize;
  static const int kSize = kYieldFromOffset + kPointerSize;

  RAW_OBJECT_COMMON(Generator);
};

class RawCoroutine : public RawGeneratorBase {
 public:
  // Layout.
  static const int kAwaitOffset = RawGeneratorBase::kSize;
  static const int kOriginOffset = kAwaitOffset + kPointerSize;
  static const int kSize = kOriginOffset + kPointerSize;

  RAW_OBJECT_COMMON(Coroutine);
};

class RawCoroutineWrapper : public RawInstance {
 public:
  RawObject coroutine() const;
  void setCoroutine(RawObject coroutine) const;

  // Layout.
  static const int kCoroutineOffset = RawHeapObject::kSize;
  static const int kSize = kCoroutineOffset + kPointerSize;

  RAW_OBJECT_COMMON(CoroutineWrapper);
};

class RawAsyncGenerator : public RawGeneratorBase {
 public:
  RawObject finalizer() const;
  void setFinalizer(RawObject finalizer) const;

  bool hooksInited() const;
  void setHooksInited(bool hooks_inited) const;

  // Layout.
  static const int kFinalizerOffset = RawGeneratorBase::kSize;
  static const int kHooksInitedOffset = kFinalizerOffset + kPointerSize;
  static const int kSize = kHooksInitedOffset + kPointerSize;

  RAW_OBJECT_COMMON(AsyncGenerator);
};

class RawAsyncGeneratorOpIterBase : public RawInstance {
 public:
  enum class State : word { Init = 0, Iter = 1, Closed = 2 };

  RawObject generator() const;
  void setGenerator(RawObject generator) const;

  State state() const;
  void setState(State state) const;

  // Layout.
  static const int kGeneratorOffset = RawHeapObject::kSize;
  static const int kStateOffset = kGeneratorOffset + kPointerSize;
  static const int kSize = kStateOffset + kPointerSize;

  RAW_OBJECT_COMMON(AsyncGeneratorOpIterBase);
};

class RawAsyncGeneratorAclose : public RawAsyncGeneratorOpIterBase {
 public:
  // Layout.
  static const int kSize = RawAsyncGeneratorOpIterBase::kSize;

  RAW_OBJECT_COMMON(AsyncGeneratorAclose);
};

class RawAsyncGeneratorAsend : public RawAsyncGeneratorOpIterBase {
 public:
  RawObject value() const;
  void setValue(RawObject value) const;

  // Layout.
  static const int kValueOffset = RawAsyncGeneratorOpIterBase::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON(AsyncGeneratorAsend);
};

class RawAsyncGeneratorAthrow : public RawAsyncGeneratorOpIterBase {
 public:
  RawObject exceptionTraceback() const;
  void setExceptionTraceback(RawObject traceback) const;

  RawObject exceptionType() const;
  void setExceptionType(RawObject exception_type) const;

  RawObject exceptionValue() const;
  void setExceptionValue(RawObject exception_value) const;

  // Layout.
  static const int kExceptionTracebackOffset =
      RawAsyncGeneratorOpIterBase::kSize;
  static const int kExceptionTypeOffset =
      kExceptionTracebackOffset + kPointerSize;
  static const int kExceptionValueOffset = kExceptionTypeOffset + kPointerSize;
  static const int kSize = kExceptionValueOffset + kPointerSize;

  RAW_OBJECT_COMMON(AsyncGeneratorAthrow);
};

class RawAsyncGeneratorWrappedValue : public RawInstance {
 public:
  RawObject value() const;
  void setValue(RawObject value) const;

  // Layout.
  static const int kValueOffset = RawHeapObject::kSize;
  static const int kSize = kValueOffset + kPointerSize;

  RAW_OBJECT_COMMON(AsyncGeneratorWrappedValue);
};

class RawTraceback : public RawInstance {
 public:
  RawObject function() const;
  void setFunction(RawObject function) const;

  RawObject lasti() const;
  void setLasti(RawObject lasti) const;

  RawObject lineno() const;
  void setLineno(RawObject lineno) const;

  RawObject next() const;
  void setNext(RawObject next) const;

  // Layout.
  static const int kNextOffset = RawHeapObject::kSize;
  static const int kFunctionOffset = kNextOffset + kPointerSize;
  static const int kLastiOffset = kFunctionOffset + kPointerSize;
  static const int kLinenoOffset = kLastiOffset + kPointerSize;
  static const int kSize = kLinenoOffset + kPointerSize;

  RAW_OBJECT_COMMON(Traceback);
};

// The primitive IO types

class RawUnderIOBase : public RawInstance {
 public:
  // Getters and setters
  bool closed() const;
  void setClosed(bool closed) const;

  // Layout
  static const int kClosedOffset = RawHeapObject::kSize;
  static const int kSize = kClosedOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UnderIOBase);
};

class RawUnderRawIOBase : public RawUnderIOBase {
 public:
  RAW_OBJECT_COMMON_NO_CAST(UnderRawIOBase);
};

class RawUnderBufferedIOBase : public RawUnderRawIOBase {
 public:
  RAW_OBJECT_COMMON_NO_CAST(UnderBufferedIOBase);
};

class RawUnderBufferedIOMixin : public RawUnderBufferedIOBase {
 public:
  // Getters and setters
  RawObject underlying() const;
  void setUnderlying(RawObject value) const;

  // Layout
  static const int kUnderlyingOffset = RawUnderBufferedIOBase::kSize;
  static const int kSize = kUnderlyingOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(UnderBufferedIOMixin);
};

class RawBufferedRandom : public RawUnderBufferedIOMixin {
 public:
  // Getters and setters
  word bufferSize() const;
  void setBufferSize(word buffer_size) const;
  RawObject reader() const;
  void setReader(RawObject reader) const;
  RawObject writeBuf() const;
  void setWriteBuf(RawObject under_write_buf) const;
  RawObject writeLock() const;
  void setWriteLock(RawObject under_write_lock) const;

  // Layout
  static const int kBufferSizeOffset = RawUnderBufferedIOMixin::kSize;
  static const int kReaderOffset = kBufferSizeOffset + kPointerSize;
  static const int kWriteBufOffset = kReaderOffset + kPointerSize;
  static const int kWriteLockOffset = kWriteBufOffset + kPointerSize;
  static const int kSize = kWriteLockOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(BufferedRandom);
};

class RawBufferedReader : public RawUnderBufferedIOMixin {
 public:
  // Getters and setters
  word bufferSize() const;
  void setBufferSize(word buffer_size) const;
  RawObject readBuf() const;
  void setReadBuf(RawObject read_buf) const;
  word readPos() const;
  void setReadPos(word read_pos) const;
  word bufferNumBytes() const;
  void setBufferNumBytes(word buffer_num_bytes) const;

  // Layout
  static const int kBufferSizeOffset = RawUnderBufferedIOMixin::kSize;
  static const int kReadBufOffset = kBufferSizeOffset + kPointerSize;
  static const int kReadPosOffset = kReadBufOffset + kPointerSize;
  static const int kBufferNumBytesOffset = kReadPosOffset + kPointerSize;
  static const int kSize = kBufferNumBytesOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(BufferedReader);
};

class RawBufferedWriter : public RawUnderBufferedIOMixin {
 public:
  // Getters and setters
  word bufferSize() const;
  void setBufferSize(RawObject buffer_size) const;
  RawObject writeBuf() const;
  void setWriteBuf(RawObject under_write_buf) const;
  RawObject writeLock() const;
  void setWriteLock(RawObject under_write_lock) const;

  // Layout
  static const int kBufferSizeOffset = RawUnderBufferedIOMixin::kSize;
  static const int kWriteBufOffset = kBufferSizeOffset + kPointerSize;
  static const int kWriteLockOffset = kWriteBufOffset + kPointerSize;
  static const int kSize = kWriteLockOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(BufferedWriter);
};

class RawBytesIO : public RawUnderBufferedIOBase {
 public:
  // Getters and setters
  RawObject buffer() const;
  void setBuffer(RawObject buffer) const;
  word numItems() const;
  void setNumItems(word num_items) const;
  word pos() const;
  void setPos(word pos) const;

  // Layout
  static const int kBufferOffset = RawUnderBufferedIOBase::kSize;
  static const int kNumItemsOffset = kBufferOffset + kPointerSize;
  static const int kPosOffset = kNumItemsOffset + kPointerSize;
  static const int kSize = kPosOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(BytesIO);
};

class RawFileIO : public RawUnderRawIOBase {
 public:
  // Getters and setters
  RawObject fd() const;
  void setFd(RawObject fd) const;

  RawObject name() const;
  void setName(RawObject value) const;

  RawObject isCreated() const;
  void setCreated(RawObject value) const;

  RawObject isReadable() const;
  void setReadable(RawObject value) const;

  RawObject isWritable() const;
  void setWritable(RawObject value) const;

  RawObject isAppending() const;
  void setAppending(RawObject value) const;

  RawObject seekable() const;
  void setSeekable(RawObject value) const;

  RawObject shouldCloseFd() const;
  void setShouldCloseFd(RawObject value) const;

  // Layout
  static const int kFdOffset = RawUnderRawIOBase::kSize;
  static const int kNameOffset = kFdOffset + kPointerSize;
  static const int kCreatedOffset = kNameOffset + kPointerSize;
  static const int kReadableOffset = kCreatedOffset + kPointerSize;
  static const int kWritableOffset = kReadableOffset + kPointerSize;
  static const int kAppendingOffset = kWritableOffset + kPointerSize;
  static const int kSeekableOffset = kAppendingOffset + kPointerSize;
  static const int kCloseFdOffset = kSeekableOffset + kPointerSize;
  static const int kSize = kCloseFdOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(FileIO);
};

class RawInstanceMethod : public RawInstance {
 public:
  // Getters and setters

  RawObject function() const;
  void setFunction(RawObject function) const;

  // Layout.
  static const int kFunctionOffset = RawHeapObject::kSize;
  static const int kSize = kFunctionOffset + kPointerSize;

  RAW_OBJECT_COMMON(InstanceMethod);
};

class RawInstanceProxy : public RawInstance {
 public:
  // Getters and setters
  RawObject instance() const;
  void setInstance(RawObject instance) const;

  // Layout
  static const int kInstanceOffset = RawHeapObject::kSize;
  static const int kSize = kInstanceOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(InstanceProxy);
};

class RawIncrementalNewlineDecoder : public RawInstance {
 public:
  // Getters and setters
  RawObject errors() const;
  void setErrors(RawObject errors) const;
  RawObject translate() const;
  void setTranslate(RawObject translate) const;
  RawObject decoder() const;
  void setDecoder(RawObject decoder) const;
  RawObject seennl() const;
  void setSeennl(RawObject seennl) const;
  RawObject pendingcr() const;
  void setPendingcr(RawObject pendingcr) const;

  // Layout
  static const int kErrorsOffset = RawHeapObject::kSize;
  static const int kTranslateOffset = kErrorsOffset + kPointerSize;
  static const int kDecoderOffset = kTranslateOffset + kPointerSize;
  static const int kSeennlOffset = kDecoderOffset + kPointerSize;
  static const int kPendingcrOffset = kSeennlOffset + kPointerSize;
  static const int kSize = kPendingcrOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(IncrementalNewlineDecoder);
};

// RawUnderTextIOBase

class RawUnderTextIOBase : public RawUnderIOBase {
 public:
  RAW_OBJECT_COMMON_NO_CAST(UnderTextIOBase);
};

// RawTextIOWrapper

class RawTextIOWrapper : public RawUnderTextIOBase {
 public:
  // Getters and setters
  RawObject buffer() const;
  void setBuffer(RawObject buffer) const;
  bool detached() const;
  bool lineBuffering() const;
  void setLineBuffering(RawObject line_buffering) const;
  RawObject encoding() const;
  void setEncoding(RawObject encoding) const;
  RawObject errors() const;
  void setErrors(RawObject errors) const;
  bool readuniversal() const;
  void setReaduniversal(RawObject readuniversal) const;
  bool readtranslate() const;
  void setReadtranslate(RawObject readtranslate) const;
  RawObject readnl() const;
  void setReadnl(RawObject readnl) const;
  bool writetranslate() const;
  void setWritetranslate(RawObject writetranslate) const;
  RawObject writenl() const;
  void setWritenl(RawObject writenl) const;
  RawObject encoder() const;
  void setEncoder(RawObject encoder) const;
  RawObject decoder() const;
  void setDecoder(RawObject decoder) const;
  RawObject decodedChars() const;
  void setDecodedChars(RawObject decoded_chars) const;
  RawObject decodedCharsUsed() const;
  void setDecodedCharsUsed(RawObject decoded_chars_used) const;
  RawObject snapshot() const;
  void setSnapshot(RawObject snapshot) const;
  RawObject seekable() const;
  void setSeekable(RawObject seekable) const;
  RawObject hasRead1() const;
  void setHasRead1(RawObject has_read1) const;
  RawObject b2cratio() const;
  void setB2cratio(RawObject b2cratio) const;
  RawObject telling() const;
  void setTelling(RawObject telling) const;

  // Layout
  static const int kBufferOffset = RawUnderTextIOBase::kSize;
  static const int kLineBufferingOffset = kBufferOffset + kPointerSize;
  static const int kEncodingOffset = kLineBufferingOffset + kPointerSize;
  static const int kErrorsOffset = kEncodingOffset + kPointerSize;
  static const int kReaduniversalOffset = kErrorsOffset + kPointerSize;
  static const int kReadtranslateOffset = kReaduniversalOffset + kPointerSize;
  static const int kReadnlOffset = kReadtranslateOffset + kPointerSize;
  static const int kWritetranslateOffset = kReadnlOffset + kPointerSize;
  static const int kWritenlOffset = kWritetranslateOffset + kPointerSize;
  static const int kEncoderOffset = kWritenlOffset + kPointerSize;
  static const int kDecoderOffset = kEncoderOffset + kPointerSize;
  static const int kDecodedCharsOffset = kDecoderOffset + kPointerSize;
  static const int kDecodedCharsUsedOffset = kDecodedCharsOffset + kPointerSize;
  static const int kSnapshotOffset = kDecodedCharsUsedOffset + kPointerSize;
  static const int kSeekableOffset = kSnapshotOffset + kPointerSize;
  static const int kHasRead1Offset = kSeekableOffset + kPointerSize;
  static const int kB2cratioOffset = kHasRead1Offset + kPointerSize;
  static const int kTellingOffset = kB2cratioOffset + kPointerSize;
  // TODO(T54575279): make mode an overflow attribute
  static const int kModeOffset = kTellingOffset + kPointerSize;
  static const int kSize = kModeOffset + kPointerSize;

  RAW_OBJECT_COMMON_NO_CAST(TextIOWrapper);
};

// RawStringIO

class RawStringIO : public RawUnderTextIOBase {
 public:
  RawObject buffer() const;
  void setBuffer(RawObject buffer) const;

  word pos() const;
  void setPos(word new_pos) const;

  // TODO(T59697642): don't use a whole attribute, just read and write a bit in
  // a bitfield.
  RawObject readnl() const;
  void setReadnl(RawObject readnl) const;

  // TODO(T59697642): don't use a whole attribute, just read and write a bit in
  // a bitfield.
  bool hasReadtranslate() const;
  void setReadtranslate(bool readtranslate) const;

  // TODO(T59697642): don't use a whole attribute, just read and write a bit in
  // a bitfield.
  bool hasReaduniversal() const;
  void setReaduniversal(bool readuniversal) const;

  // TODO(T59697642): don't use a whole attribute, just read and write bits in a
  // bitfield.
  RawObject seennl() const;
  void setSeennl(RawObject seennl) const;

  // TODO(T59697642): don't use a whole attribute, just read and write a bit in
  // a bitfield.
  RawObject writenl() const;
  void setWritenl(RawObject writenl) const;

  // TODO(T59697642): don't use a whole attribute, just read and write a bit in
  // a bitfield.
  bool hasWritetranslate() const;
  void setWritetranslate(bool writetranslate) const;

  // Layout
  static const int kBufferOffset = RawUnderTextIOBase::kSize;
  static const int kPosOffset = kBufferOffset + kPointerSize;
  static const int kReadnlOffset = kPosOffset + kPointerSize;
  static const int kReadtranslateOffset = kReadnlOffset + kPointerSize;
  static const int kReaduniversalOffset = kReadtranslateOffset + kPointerSize;
  static const int kSeennlOffset = kReaduniversalOffset + kPointerSize;
  static const int kWritenlOffset = kSeennlOffset + kPointerSize;
  static const int kWritetranslateOffset = kWritenlOffset + kPointerSize;
  static const int kSize = kWritetranslateOffset + kPointerSize;

  RAW_OBJECT_COMMON(StringIO);
};

// RawObject

inline word roundAllocationSize(word size) {
  return Utils::roundUp(size, kObjectAlignment);
}

inline bool isInstanceLayout(LayoutId id) {
  return id > LayoutId::kLastNonInstance;
}

inline RawObject::RawObject(uword raw) : raw_{raw} {}

inline uword RawObject::raw() const { return raw_; }

inline bool RawObject::isObject() const { return true; }

inline LayoutId RawObject::layoutId() const {
  if (isHeapObject()) {
    return RawHeapObject::cast(*this).header().layoutId();
  }
  if (isSmallInt()) {
    return LayoutId::kSmallInt;
  }
  return static_cast<LayoutId>(raw() & kImmediateTagMask);
}

inline bool RawObject::isBool() const {
  return (raw() & RawBool::kTagMask) == kBoolTag;
}

inline bool RawObject::isError() const {
  return (raw() & RawError::kTagMask) == kErrorTag;
}

inline bool RawObject::isErrorError() const {
  return raw() == RawError::error().raw();
}

inline bool RawObject::isErrorException() const {
  return raw() == RawError::exception().raw();
}

inline bool RawObject::isErrorNotFound() const {
  return raw() == RawError::notFound().raw();
}

inline bool RawObject::isErrorOutOfBounds() const {
  return raw() == RawError::outOfBounds().raw();
}

inline bool RawObject::isErrorOutOfMemory() const {
  return raw() == RawError::outOfMemory().raw();
}

inline bool RawObject::isErrorNoMoreItems() const {
  return raw() == RawError::noMoreItems().raw();
}

inline bool RawObject::isHeader() const {
  return (raw() & kPrimaryTagMask) == kHeaderTag;
}

inline bool RawObject::isNoneType() const {
  return *this == RawNoneType::object();
}

inline bool RawObject::isNotImplementedType() const {
  return *this == RawNotImplementedType::object();
}

inline bool RawObject::isSmallBytes() const {
  return (raw() & kImmediateTagMask) == kSmallBytesTag;
}

inline bool RawObject::isSmallInt() const {
  return (raw() & kSmallIntTagMask) == kSmallIntTag;
}

inline bool RawObject::isSmallStr() const {
  return (raw() & kImmediateTagMask) == kSmallStrTag;
}

inline bool RawObject::isUnbound() const {
  return *this == RawUnbound::object();
}

inline bool RawObject::isHeapObject() const {
  return (raw() & kPrimaryTagMask) == kHeapObjectTag;
}

inline bool RawObject::isHeapObjectWithLayout(LayoutId layout_id) const {
  return isHeapObject() &&
         RawHeapObject::cast(*this).header().layoutId() == layout_id;
}

inline bool RawObject::isInternal() const {
  // Test whether an object is unsafe to expose to managed code
  return isError() || isMutableBytes() || isMutableTuple() || isLayout() ||
         isUnbound();
}

inline bool RawObject::isImmediateObjectNotSmallInt() const {
  // Test whether object is not a heap object when it is known that it is not a
  // SmallInt (the lowest bit is guaranteed to be one so we don't need to
  // re-test that).
  static_assert((kHeapObjectTag & ~kSmallIntTagMask) == 0,
                "assumed heapobject tag bits outside smallint bit are 0");
  return (raw() & (kPrimaryTagMask & ~kSmallIntTagMask)) != 0;
}

inline bool RawObject::isInstance() const {
  return isHeapObject() && (RawHeapObject::cast(*this).header().layoutId() >
                            LayoutId::kLastNonInstance);
}

inline bool RawObject::isArray() const {
  return isHeapObjectWithLayout(LayoutId::kArray);
}

inline bool RawObject::isAsyncGenerator() const {
  return isHeapObjectWithLayout(LayoutId::kAsyncGenerator);
}

inline bool RawObject::isAsyncGeneratorOpIterBase() const {
  return isAsyncGeneratorAclose() || isAsyncGeneratorAsend() ||
         isAsyncGeneratorAthrow();
}

inline bool RawObject::isAsyncGeneratorAclose() const {
  return isHeapObjectWithLayout(LayoutId::kAsyncGeneratorAclose);
}

inline bool RawObject::isAsyncGeneratorAsend() const {
  return isHeapObjectWithLayout(LayoutId::kAsyncGeneratorAsend);
}

inline bool RawObject::isAsyncGeneratorAthrow() const {
  return isHeapObjectWithLayout(LayoutId::kAsyncGeneratorAthrow);
}

inline bool RawObject::isAsyncGeneratorWrappedValue() const {
  return isHeapObjectWithLayout(LayoutId::kAsyncGeneratorWrappedValue);
}

inline bool RawObject::isAttributeDict() const {
  return isType() || isModule();
}

inline bool RawObject::isBaseException() const {
  return isHeapObjectWithLayout(LayoutId::kBaseException);
}

inline bool RawObject::isBoundMethod() const {
  return isHeapObjectWithLayout(LayoutId::kBoundMethod);
}

inline bool RawObject::isBufferedRandom() const {
  return isHeapObjectWithLayout(LayoutId::kBufferedRandom);
}

inline bool RawObject::isBufferedReader() const {
  return isHeapObjectWithLayout(LayoutId::kBufferedReader);
}

inline bool RawObject::isBufferedWriter() const {
  return isHeapObjectWithLayout(LayoutId::kBufferedWriter);
}

inline bool RawObject::isUnderBufferedIOBase() const {
  return isHeapObjectWithLayout(LayoutId::kUnderBufferedIOBase);
}

inline bool RawObject::isUnderBufferedIOMixin() const {
  return isHeapObjectWithLayout(LayoutId::kUnderBufferedIOMixin);
}

inline bool RawObject::isBytearray() const {
  return isHeapObjectWithLayout(LayoutId::kBytearray);
}

inline bool RawObject::isBytearrayIterator() const {
  return isHeapObjectWithLayout(LayoutId::kBytearrayIterator);
}

inline bool RawObject::isBytesIO() const {
  return isHeapObjectWithLayout(LayoutId::kBytesIO);
}

inline bool RawObject::isBytesIterator() const {
  return isHeapObjectWithLayout(LayoutId::kBytesIterator);
}

inline bool RawObject::isCell() const {
  return isHeapObjectWithLayout(LayoutId::kCell);
}

inline bool RawObject::isClassMethod() const {
  return isHeapObjectWithLayout(LayoutId::kClassMethod);
}

inline bool RawObject::isCode() const {
  return isHeapObjectWithLayout(LayoutId::kCode);
}

inline bool RawObject::isComplex() const {
  return isHeapObjectWithLayout(LayoutId::kComplex);
}

inline bool RawObject::isContext() const {
  return isHeapObjectWithLayout(LayoutId::kContext);
}

inline bool RawObject::isContextVar() const {
  return isHeapObjectWithLayout(LayoutId::kContextVar);
}

inline bool RawObject::isCoroutine() const {
  return isHeapObjectWithLayout(LayoutId::kCoroutine);
}

inline bool RawObject::isCoroutineWrapper() const {
  return isHeapObjectWithLayout(LayoutId::kCoroutineWrapper);
}

inline bool RawObject::isDataArray() const {
  return isLargeBytes() || isLargeStr() || isMutableBytes();
}

inline bool RawObject::isDeque() const {
  return isHeapObjectWithLayout(LayoutId::kDeque);
}

inline bool RawObject::isDequeIterator() const {
  return isHeapObjectWithLayout(LayoutId::kDequeIterator);
}

inline bool RawObject::isDequeReverseIterator() const {
  return isHeapObjectWithLayout(LayoutId::kDequeReverseIterator);
}

inline bool RawObject::isDict() const {
  return isHeapObjectWithLayout(LayoutId::kDict);
}

inline bool RawObject::isDictItemIterator() const {
  return isHeapObjectWithLayout(LayoutId::kDictItemIterator);
}

inline bool RawObject::isDictItems() const {
  return isHeapObjectWithLayout(LayoutId::kDictItems);
}

inline bool RawObject::isDictKeyIterator() const {
  return isHeapObjectWithLayout(LayoutId::kDictKeyIterator);
}

inline bool RawObject::isDictKeys() const {
  return isHeapObjectWithLayout(LayoutId::kDictKeys);
}

inline bool RawObject::isDictValueIterator() const {
  return isHeapObjectWithLayout(LayoutId::kDictValueIterator);
}

inline bool RawObject::isDictValues() const {
  return isHeapObjectWithLayout(LayoutId::kDictValues);
}

inline bool RawObject::isEllipsis() const {
  return isHeapObjectWithLayout(LayoutId::kEllipsis);
}

inline bool RawObject::isEnumerate() const {
  return isHeapObjectWithLayout(LayoutId::kEnumerate);
}

inline bool RawObject::isException() const {
  return isHeapObjectWithLayout(LayoutId::kException);
}

inline bool RawObject::isExceptionState() const {
  return isHeapObjectWithLayout(LayoutId::kExceptionState);
}

inline bool RawObject::isFileIO() const {
  return isHeapObjectWithLayout(LayoutId::kFileIO);
}

inline bool RawObject::isFloat() const {
  return isHeapObjectWithLayout(LayoutId::kFloat);
}

inline bool RawObject::isFrameProxy() const {
  return isHeapObjectWithLayout(LayoutId::kFrameProxy);
}

inline bool RawObject::isFrozenSet() const {
  return isHeapObjectWithLayout(LayoutId::kFrozenSet);
}

inline bool RawObject::isFunction() const {
  return isHeapObjectWithLayout(LayoutId::kFunction);
}

inline bool RawObject::isGenerator() const {
  return isHeapObjectWithLayout(LayoutId::kGenerator);
}

inline bool RawObject::isGeneratorFrame() const {
  return isHeapObjectWithLayout(LayoutId::kGeneratorFrame);
}

inline bool RawObject::isIncrementalNewlineDecoder() const {
  return isHeapObjectWithLayout(LayoutId::kIncrementalNewlineDecoder);
}

inline bool RawObject::isInstanceMethod() const {
  return isHeapObjectWithLayout(LayoutId::kInstanceMethod);
}

inline bool RawObject::isInstanceProxy() const {
  return isHeapObjectWithLayout(LayoutId::kInstanceProxy);
}

inline bool RawObject::isImportError() const {
  return isHeapObjectWithLayout(LayoutId::kImportError);
}

inline bool RawObject::isIndexError() const {
  return isHeapObjectWithLayout(LayoutId::kIndexError);
}

inline bool RawObject::isUnderIOBase() const {
  return isHeapObjectWithLayout(LayoutId::kUnderIOBase);
}

inline bool RawObject::isKeyError() const {
  return isHeapObjectWithLayout(LayoutId::kKeyError);
}

inline bool RawObject::isLargeBytes() const {
  return isHeapObjectWithLayout(LayoutId::kLargeBytes) || isMutableBytes();
}

inline bool RawObject::isLargeInt() const {
  return isHeapObjectWithLayout(LayoutId::kLargeInt);
}

inline bool RawObject::isLargeStr() const {
  return isHeapObjectWithLayout(LayoutId::kLargeStr);
}

inline bool RawObject::isLayout() const {
  return isHeapObjectWithLayout(LayoutId::kLayout);
}

inline bool RawObject::isList() const {
  return isHeapObjectWithLayout(LayoutId::kList);
}

inline bool RawObject::isListIterator() const {
  return isHeapObjectWithLayout(LayoutId::kListIterator);
}

inline bool RawObject::isLongRangeIterator() const {
  return isHeapObjectWithLayout(LayoutId::kLongRangeIterator);
}

inline bool RawObject::isLookupError() const {
  return isHeapObjectWithLayout(LayoutId::kLookupError);
}

inline bool RawObject::isMappingProxy() const {
  return isHeapObjectWithLayout(LayoutId::kMappingProxy);
}

inline bool RawObject::isMemoryView() const {
  return isHeapObjectWithLayout(LayoutId::kMemoryView);
}

inline bool RawObject::isMmap() const {
  return isHeapObjectWithLayout(LayoutId::kMmap);
}

inline bool RawObject::isModule() const {
  return isHeapObjectWithLayout(LayoutId::kModule);
}

inline bool RawObject::isModuleProxy() const {
  return isHeapObjectWithLayout(LayoutId::kModuleProxy);
}

inline bool RawObject::isModuleNotFoundError() const {
  return isHeapObjectWithLayout(LayoutId::kModuleNotFoundError);
}

inline bool RawObject::isMutableBytes() const {
  return isHeapObjectWithLayout(LayoutId::kMutableBytes);
}

inline bool RawObject::isMutableTuple() const {
  return isHeapObjectWithLayout(LayoutId::kMutableTuple);
}

inline bool RawObject::isNotImplementedError() const {
  return isHeapObjectWithLayout(LayoutId::kNotImplementedError);
}

inline bool RawObject::isPointer() const {
  return isHeapObjectWithLayout(LayoutId::kPointer);
}

inline bool RawObject::isProperty() const {
  return isHeapObjectWithLayout(LayoutId::kProperty);
}

inline bool RawObject::isRange() const {
  return isHeapObjectWithLayout(LayoutId::kRange);
}

inline bool RawObject::isRangeIterator() const {
  return isHeapObjectWithLayout(LayoutId::kRangeIterator);
}

inline bool RawObject::isUnderRawIOBase() const {
  return isHeapObjectWithLayout(LayoutId::kUnderRawIOBase);
}

inline bool RawObject::isRuntimeError() const {
  return isHeapObjectWithLayout(LayoutId::kRuntimeError);
}

inline bool RawObject::isSeqIterator() const {
  return isHeapObjectWithLayout(LayoutId::kSeqIterator);
}

inline bool RawObject::isSet() const {
  return isHeapObjectWithLayout(LayoutId::kSet);
}

inline bool RawObject::isSetIterator() const {
  return isHeapObjectWithLayout(LayoutId::kSetIterator);
}

inline bool RawObject::isSlice() const {
  return isHeapObjectWithLayout(LayoutId::kSlice);
}

inline bool RawObject::isSlotDescriptor() const {
  return isHeapObjectWithLayout(LayoutId::kSlotDescriptor);
}

inline bool RawObject::isStaticMethod() const {
  return isHeapObjectWithLayout(LayoutId::kStaticMethod);
}

inline bool RawObject::isStopIteration() const {
  return isHeapObjectWithLayout(LayoutId::kStopIteration);
}

inline bool RawObject::isStrArray() const {
  return isHeapObjectWithLayout(LayoutId::kStrArray);
}

inline bool RawObject::isStringIO() const {
  return isHeapObjectWithLayout(LayoutId::kStringIO);
}

inline bool RawObject::isStrIterator() const {
  return isHeapObjectWithLayout(LayoutId::kStrIterator);
}

inline bool RawObject::isSuper() const {
  return isHeapObjectWithLayout(LayoutId::kSuper);
}

inline bool RawObject::isSyntaxError() const {
  return isHeapObjectWithLayout(LayoutId::kSyntaxError);
}

inline bool RawObject::isSystemExit() const {
  return isHeapObjectWithLayout(LayoutId::kSystemExit);
}

inline bool RawObject::isTextIOWrapper() const {
  return isHeapObjectWithLayout(LayoutId::kTextIOWrapper);
}

inline bool RawObject::isToken() const {
  return isHeapObjectWithLayout(LayoutId::kToken);
}

inline bool RawObject::isTraceback() const {
  return isHeapObjectWithLayout(LayoutId::kTraceback);
}

inline bool RawObject::isTuple() const {
  return isHeapObjectWithLayout(LayoutId::kTuple) || isMutableTuple();
}

inline bool RawObject::isTupleIterator() const {
  return isHeapObjectWithLayout(LayoutId::kTupleIterator);
}

inline bool RawObject::isType() const {
  return isHeapObjectWithLayout(LayoutId::kType);
}

inline bool RawObject::isTypeProxy() const {
  return isHeapObjectWithLayout(LayoutId::kTypeProxy);
}

inline bool RawObject::isUnicodeDecodeError() const {
  return isHeapObjectWithLayout(LayoutId::kUnicodeDecodeError);
}

inline bool RawObject::isUnicodeEncodeError() const {
  return isHeapObjectWithLayout(LayoutId::kUnicodeEncodeError);
}

inline bool RawObject::isUnicodeError() const {
  return isHeapObjectWithLayout(LayoutId::kUnicodeError);
}

inline bool RawObject::isUnicodeErrorBase() const {
  return isUnicodeDecodeError() || isUnicodeEncodeError() ||
         isUnicodeTranslateError();
}

inline bool RawObject::isUnicodeTranslateError() const {
  return isHeapObjectWithLayout(LayoutId::kUnicodeTranslateError);
}

inline bool RawObject::isValueCell() const {
  return isHeapObjectWithLayout(LayoutId::kValueCell);
}

inline bool RawObject::isWeakCallableProxy() const {
  return isHeapObjectWithLayout(LayoutId::kWeakCallableProxy);
}

inline bool RawObject::isWeakProxy() const {
  return isHeapObjectWithLayout(LayoutId::kWeakProxy);
}

inline bool RawObject::isWeakLink() const {
  return isHeapObjectWithLayout(LayoutId::kWeakLink);
}

inline bool RawObject::isWeakRef() const {
  // WeakLink is a subclass of WeakLink sharing its layout, so this is safe.
  return isHeapObjectWithLayout(LayoutId::kWeakRef) ||
         isHeapObjectWithLayout(LayoutId::kWeakLink);
}

inline bool RawObject::isBytes() const {
  return isSmallBytes() || isLargeBytes();
}

inline bool RawObject::isGeneratorBase() const {
  return isGenerator() || isCoroutine() || isAsyncGenerator();
}

inline bool RawObject::isInt() const {
  return isSmallInt() || isLargeInt() || isBool();
}

inline bool RawObject::isSetBase() const { return isSet() || isFrozenSet(); }

inline bool RawObject::isStr() const { return isSmallStr() || isLargeStr(); }

inline bool RawObject::operator==(const RawObject& other) const {
  return raw() == other.raw();
}

inline bool RawObject::operator!=(const RawObject& other) const {
  return !operator==(other);
}

template <typename T>
T RawObject::rawCast() const {
  return *static_cast<const T*>(this);
}

// RawBytes

inline word RawBytes::findByte(byte value, word start, word length) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).findByte(value, start, length);
  }
  return RawLargeBytes::cast(*this).findByte(value, start, length);
}

inline RawBytes RawBytes::empty() {
  return RawSmallBytes::empty().rawCast<RawBytes>();
}

inline word RawBytes::length() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).length();
  }
  return RawLargeBytes::cast(*this).length();
}

ALWAYS_INLINE byte RawBytes::byteAt(word index) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).byteAt(index);
  }
  return RawLargeBytes::cast(*this).byteAt(index);
}

inline RawObject RawBytes::becomeStr() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).becomeStr();
  }
  return RawLargeBytes::cast(*this).becomeStr();
}

inline void RawBytes::copyTo(byte* dst, word length) const {
  if (isImmediateObjectNotSmallInt()) {
    RawSmallBytes::cast(*this).copyTo(dst, length);
    return;
  }
  RawLargeBytes::cast(*this).copyTo(dst, length);
}

inline void RawBytes::copyToStartAt(byte* dst, word length, word index) const {
  if (isImmediateObjectNotSmallInt()) {
    RawSmallBytes::cast(*this).copyToStartAt(dst, length, index);
    return;
  }
  RawLargeBytes::cast(*this).copyToStartAt(dst, length, index);
}

inline bool RawBytes::includesByte(byte b) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).includesByte(b);
  }
  return RawLargeBytes::cast(*this).includesByte(b);
}

inline bool RawBytes::isASCII() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).isASCII();
  }
  return RawLargeBytes::cast(*this).isASCII();
}

inline char* RawBytes::toCStr() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).toCStr();
  }
  return RawLargeBytes::cast(*this).toCStr();
}

inline uint16_t RawBytes::uint16At(word index) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).uint16At(index);
  }
  return RawLargeBytes::cast(*this).uint16At(index);
}

inline uint32_t RawBytes::uint32At(word index) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallBytes::cast(*this).uint32At(index);
  }
  return RawLargeBytes::cast(*this).uint32At(index);
}

inline uint64_t RawBytes::uint64At(word index) const {
  DCHECK(!isSmallBytes(), "uint64_t cannot fit into SmallBytes");
  return RawLargeBytes::cast(*this).uint64At(index);
}

// RawInt

inline word RawInt::asWord() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).value();
  }
  if (isBool()) {
    return RawBool::cast(*this).value();
  }
  return RawLargeInt::cast(*this).asWord();
}

inline word RawInt::asWordSaturated() const {
  if (numDigits() == 1) return asWord();
  return isNegative() ? kMinWord : kMaxWord;
}

inline void* RawInt::asCPtr() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).asCPtr();
  }
  return RawLargeInt::cast(*this).asCPtr();
}

template <typename T>
OptInt<T> RawInt::asInt() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).asInt<T>();
  }
  if (isBool()) {
    return OptInt<T>::valid(RawBool::cast(*this).value() ? 1 : 0);
  }
  return RawLargeInt::cast(*this).asInt<T>();
}

inline word RawInt::bitLength() const {
  if (isSmallInt()) {
    uword self = static_cast<uword>(std::abs(RawSmallInt::cast(*this).value()));
    return Utils::highestBit(self);
  }
  if (isBool()) {
    return RawBool::cast(*this) == RawBool::trueObj() ? 1 : 0;
  }
  return RawLargeInt::cast(*this).bitLength();
}

inline bool RawInt::isEven() const {
  if (isSmallInt()) {
    return (RawSmallInt::cast(*this).value() & 1) == 0;
  }
  if (isBool()) {
    return *this == RawBool::falseObj();
  }
  return RawLargeInt::cast(*this).isEven();
}

inline bool RawInt::isNegative() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).value() < 0;
  }
  if (isBool()) {
    return false;
  }
  return RawLargeInt::cast(*this).isNegative();
}

inline bool RawInt::isOdd() const {
  if (isSmallInt()) {
    return (RawSmallInt::cast(*this).value() & 1) != 0;
  }
  if (isBool()) {
    return *this == RawBool::trueObj();
  }
  return !RawLargeInt::cast(*this).isEven();
}

inline bool RawInt::isPositive() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).value() > 0;
  }
  if (isBool()) {
    return RawBool::cast(*this) == RawBool::trueObj();
  }
  return RawLargeInt::cast(*this).isPositive();
}

inline bool RawInt::isZero() const {
  if (isSmallInt()) {
    return RawSmallInt::cast(*this).value() == 0;
  }
  if (isBool()) {
    return RawBool::cast(*this) == RawBool::falseObj();
  }
  // A RawLargeInt can never be zero
  DCHECK(isLargeInt(), "RawObject must be a RawLargeInt");
  return false;
}

inline word RawInt::numDigits() const {
  if (isSmallInt() || isBool()) {
    return 1;
  }
  return RawLargeInt::cast(*this).numDigits();
}

inline uword RawInt::digitAt(word index) const {
  if (isSmallInt()) {
    DCHECK(index == 0, "RawSmallInt digit index out of bounds");
    return RawSmallInt::cast(*this).value();
  }
  if (isBool()) {
    DCHECK(index == 0, "RawBool digit index out of bounds");
    return RawBool::cast(*this).value();
  }
  return RawLargeInt::cast(*this).digitAt(index);
}

// RawSmallInt

inline word RawSmallInt::value() const {
  return static_cast<word>(raw()) >> kSmallIntTagBits;
}

inline void* RawSmallInt::asCPtr() const {
  return reinterpret_cast<void*>(value());
}

inline void* RawSmallInt::asAlignedCPtr() const {
  return reinterpret_cast<void*>(asReinterpretedWord());
}

inline word RawSmallInt::asReinterpretedWord() const {
  return static_cast<word>(raw());
}

template <typename T>
if_signed_t<T, OptInt<T>> RawSmallInt::asInt() const {
  static_assert(sizeof(T) <= sizeof(word), "T must not be larger than word");

  auto const value = this->value();
  if (value > std::numeric_limits<T>::max()) return OptInt<T>::overflow();
  if (value < std::numeric_limits<T>::min()) return OptInt<T>::underflow();
  return OptInt<T>::valid(value);
}

template <typename T>
if_unsigned_t<T, OptInt<T>> RawSmallInt::asInt() const {
  static_assert(sizeof(T) <= sizeof(word), "T must not be larger than word");
  auto const max = std::numeric_limits<T>::max();
  auto const value = this->value();

  if (value < 0) return OptInt<T>::underflow();
  if (max >= RawSmallInt::kMaxValue || static_cast<uword>(value) <= max) {
    return OptInt<T>::valid(value);
  }
  return OptInt<T>::overflow();
}

inline RawSmallInt RawSmallInt::fromWord(word value) {
  DCHECK(RawSmallInt::isValid(value), "invalid cast");
  return cast(RawObject{static_cast<uword>(value) << kSmallIntTagBits});
}

inline RawSmallInt RawSmallInt::fromWordTruncated(word value) {
  return cast(RawObject{static_cast<uword>(value) << kSmallIntTagBits});
}

inline RawSmallInt RawSmallInt::fromReinterpretedWord(word value) {
  return cast(RawObject{static_cast<uword>(value)});
}

inline RawSmallInt RawSmallInt::fromAlignedCPtr(void* ptr) {
  return fromReinterpretedWord(reinterpret_cast<word>(ptr));
}

inline word RawSmallInt::truncate(word value) {
  return (value << kSmallIntTagBits) >> kSmallIntTagBits;
}

inline word RawSmallInt::hash() const {
  word val = value();
  uword abs = static_cast<uword>(val);
  // Shortcut for positive values smaller than `kArithmeticHashModulus`.
  if (abs < kArithmeticHashModulus) {
    return value();
  }
  // Compute `value % kArithmeticHashModulus` (with C/C++ style modulo).  This
  // uses the algorithm from `longIntHash()` simplified for a single word.
  const word bits_per_half = kBitsPerWord / 2;
  if (val < 0) {
    abs = -abs;
  }
  // The `longIntHash()` formula is simplified using the following equivalences:
  // (1)     ((abs >> bits_per_half) & p) << bits_per_half
  //    <=>  abs & ((p >> bits_per_half) << bits_per_half)
  // (2)     (abs >> bits_per_half) >> (kArithmeticHashBits - bits_per_half)
  //    <=>  abs >> kArithmeticHashBits
  uword result =
      (abs & ((kArithmeticHashModulus >> bits_per_half) << bits_per_half)) |
      abs >> kArithmeticHashBits;
  result += abs & ((uword{1} << bits_per_half) - 1);
  if (result >= kArithmeticHashModulus) {
    result -= kArithmeticHashModulus;
  }
  if (val < 0) {
    result = -result;
    // cpython replaces `-1` results with -2, because -1 is used as an
    // "uninitialized hash" marker in some situations. We do not use the same
    // marker, but do the same to match behavior.
    if (result == static_cast<uword>(word{-1})) {
      result -= 1;
    }
  }
  return result;
}

// RawHeader

inline word RawHeader::count() const {
  return static_cast<word>((raw() >> kCountOffset) & kCountMask);
}

inline bool RawHeader::hasOverflow() const {
  return count() == kCountOverflowFlag;
}

inline word RawHeader::hashCode() const {
  return static_cast<word>((raw() >> kHashCodeOffset) & kHashCodeMask);
}

inline RawHeader RawHeader::withHashCode(word value) const {
  auto header = raw();
  header &= ~(kHashCodeMask << kHashCodeOffset);
  header |= (value & kHashCodeMask) << kHashCodeOffset;
  return cast(RawObject{header});
}

inline LayoutId RawHeader::layoutId() const {
  return static_cast<LayoutId>((raw() >> kLayoutIdOffset) & kLayoutIdMask);
}

inline RawHeader RawHeader::withLayoutId(LayoutId layout_id) const {
  DCHECK_BOUND(static_cast<word>(layout_id), kMaxLayoutId);
  auto header = raw();
  header &= ~(kLayoutIdMask << kLayoutIdOffset);
  header |= (static_cast<word>(layout_id) & kLayoutIdMask) << kLayoutIdOffset;
  return cast(RawObject{header});
}

inline ObjectFormat RawHeader::format() const {
  return static_cast<ObjectFormat>((raw() >> kFormatOffset) & kFormatMask);
}

inline RawHeader RawHeader::from(word count, word hash, LayoutId id,
                                 ObjectFormat format) {
  DCHECK(
      (count >= 0) && ((count <= kCountMax) || (count == kCountOverflowFlag)),
      "bounds violation, %ld not in 0..%d", count, kCountMax);
  uword result = kHeaderTag;
  result |= ((count > kCountMax) ? kCountOverflowFlag : count) << kCountOffset;
  result |= hash << kHashCodeOffset;
  result |= static_cast<uword>(id) << kLayoutIdOffset;
  result |= static_cast<uword>(format) << kFormatOffset;
  return cast(RawObject{result});
}

// RawSmallData

// Access data within a SmallData reference.  This gives direct low-level
// access. It is only apropriate to use in code to build up higher abstractions.
inline const byte* smallDataData(const RawSmallData* obj) {
  static_assert(endian::native == endian::little, "big endian not implemented");
  return reinterpret_cast<const byte*>(obj) + RawSmallData::kDataOffset;
}

inline RawSmallData::RawSmallData(uword raw) : RawObject(raw) {}

inline word RawSmallData::length() const {
  return (raw() >> kImmediateTagBits) & kMaxLength;
}

inline byte RawSmallData::byteAt(word index) const {
  DCHECK_INDEX(index, length());
  return smallDataData(this)[index];
}

inline void RawSmallData::copyTo(byte* dst, word length) const {
  DCHECK_BOUND(length, this->length());
  std::memcpy(dst, smallDataData(this), length);
}

inline void RawSmallData::copyToStartAt(byte* dst, word length,
                                        word index) const {
  DCHECK_BOUND(index, this->length());
  DCHECK_BOUND(length, this->length() - index);
  std::memcpy(dst, smallDataData(this) + index, length);
}

inline uint16_t RawSmallData::uint16At(word index) const {
  uint16_t result;
  DCHECK_INDEX(index, length() - word{sizeof(result) - 1});
  std::memcpy(&result, smallDataData(this) + index, sizeof(result));
  return result;
}

inline uint32_t RawSmallData::uint32At(word index) const {
  uint32_t result;
  DCHECK(kMaxLength >= sizeof(result), "SmallBytes cannot fit uint32_t");
  DCHECK_INDEX(index, length() - word{sizeof(result) - 1});
  std::memcpy(&result, smallDataData(this) + index, sizeof(result));
  return result;
}

inline word RawSmallData::hash() const {
  return static_cast<word>(raw() >> RawObject::kImmediateTagBits);
}

// RawSmallBytes

inline RawSmallBytes::RawSmallBytes(uword raw) : RawSmallData(raw) {}

inline RawSmallBytes RawSmallBytes::empty() {
  return RawSmallBytes(kSmallBytesTag);
}

// RawSmallStr

inline RawSmallStr::RawSmallStr(uword raw) : RawSmallData(raw) {}

inline RawSmallStr RawSmallStr::empty() { return RawSmallStr(kSmallStrTag); }

// RawError

inline RawError::RawError(ErrorKind kind)
    : RawObject{(static_cast<uword>(kind) << kKindOffset) | kErrorTag} {}

inline RawError RawError::error() { return RawError{ErrorKind::kNone}; }

inline RawError RawError::exception() {
  return RawError{ErrorKind::kException};
}

inline RawError RawError::notFound() { return RawError{ErrorKind::kNotFound}; }

inline RawError RawError::noMoreItems() {
  return RawError{ErrorKind::kNoMoreItems};
}

inline RawError RawError::outOfMemory() {
  return RawError{ErrorKind::kOutOfMemory};
}

inline RawError RawError::outOfBounds() {
  return RawError{ErrorKind::kOutOfBounds};
}

inline ErrorKind RawError::kind() const {
  return static_cast<ErrorKind>(raw() >> kKindOffset);
}

// RawBool

inline RawBool RawBool::trueObj() { return fromBool(true); }

inline RawBool RawBool::falseObj() { return fromBool(false); }

inline word RawBool::hash() const { return value(); }

inline RawBool RawBool::negate(RawObject value) {
  DCHECK(value.isBool(), "not a boolean instance");
  return (value == trueObj()) ? falseObj() : trueObj();
}

inline RawBool RawBool::fromBool(bool value) {
  return cast(
      RawObject{(static_cast<uword>(value) << kValueOffset) | kBoolTag});
}

inline bool RawBool::value() const {
  return static_cast<byte>(raw() >> kValueOffset) ? true : false;
}

// RawNotImplementedType

inline RawNotImplementedType RawNotImplementedType::object() {
  return RawObject{kNotImplementedTag}.rawCast<RawNotImplementedType>();
}

// RawUnbound

inline RawUnbound RawUnbound::object() {
  return RawObject{kUnboundTag}.rawCast<RawUnbound>();
}

// RawNoneType

inline RawNoneType RawNoneType::object() {
  return RawObject{kMaxUword}.rawCast<RawNoneType>();
}

// RawHeapObject

inline uword RawHeapObject::address() const { return raw() - kHeapObjectTag; }

inline uword RawHeapObject::baseAddress() const {
  uword result = address() - RawHeader::kSize;
  if (header().hasOverflow()) {
    result -= kPointerSize;
  }
  return result;
}

inline RawHeader RawHeapObject::header() const {
  return *reinterpret_cast<RawHeader*>(address() + kHeaderOffset);
}

inline void RawHeapObject::setHeader(RawHeader header) const {
  *reinterpret_cast<RawHeader*>(address() + kHeaderOffset) = header;
}

inline word RawHeapObject::headerOverflow() const {
  DCHECK(header().hasOverflow(), "expected Overflow");
  return reinterpret_cast<RawSmallInt*>(address() + kHeaderOverflowOffset)
      ->value();
}

inline RawHeapObject RawHeapObject::initializeHeader(uword address, word count,
                                                     word hash, LayoutId id,
                                                     ObjectFormat format) {
  if (count > RawHeader::kCountMax) {
    *reinterpret_cast<RawSmallInt*>(address) = RawSmallInt::fromWord(count);
    address += kPointerSize;
    count = RawHeader::kCountOverflowFlag;
  }
  *reinterpret_cast<RawHeader*>(address) =
      RawHeader::from(count, hash, id, format);
  address += kPointerSize;
  return fromAddress(address);
}

inline RawHeapObject RawHeapObject::fromAddress(uword address) {
  DCHECK((address & kPrimaryTagMask) == 0,
         "invalid cast, expected heap address");
  return cast(RawObject{address + kHeapObjectTag});
}

inline word RawHeapObject::headerCountOrOverflow() const {
  if (header().hasOverflow()) {
    return headerOverflow();
  }
  return header().count();
}

inline word RawHeapObject::size() const {
  word count = headerCountOrOverflow();
  word result = headerSize(count);
  switch (header().format()) {
    case ObjectFormat::kData:
      result += count;
      break;
    case ObjectFormat::kObjects:
      result += count * kPointerSize;
      break;
  }
  return roundAllocationSize(result);
}

inline word RawHeapObject::headerSize(word count) {
  word result = kPointerSize;
  if (count > RawHeader::kCountMax) {
    result += kPointerSize;
  }
  return result;
}

inline RawObject RawInstance::initializeWithNone(uword address,
                                                 word num_attributes,
                                                 LayoutId layout_id) {
  RawHeapObject result = RawHeapObject::initializeHeader(
      address, /*count=*/num_attributes, /*hash=*/0, layout_id,
      ObjectFormat::kObjects);
  word start = RawHeapObject::kSize;
  word size = num_attributes * kPointerSize;
  std::memset(reinterpret_cast<byte*>(result.address() + start), -1,
              size - start);
  return result;
}

inline RawObject RawInstance::initializeWithZero(uword address,
                                                 word num_attributes,
                                                 LayoutId layout_id) {
  // No memset necessary here, as the memory is guaranteed to be zero already.
  return RawHeapObject::initializeHeader(address, /*count=*/num_attributes,
                                         /*hash=*/0, layout_id,
                                         ObjectFormat::kObjects);
}

inline bool RawHeapObject::isRoot() const {
  return header().format() == ObjectFormat::kObjects;
}

inline bool RawHeapObject::isForwarding() const {
  // In case of forwarded objects the header was replaced with a reference
  // to the forwarded HeapObject.
  return !reinterpret_cast<RawObject*>(address() + kHeaderOffset)->isHeader();
}

inline RawObject RawHeapObject::forward() const {
  return *reinterpret_cast<RawObject*>(address() + kHeaderOffset);
}

inline void RawHeapObject::forwardTo(RawObject object) const {
  // Overwrite the header with the forwarding address.
  *reinterpret_cast<RawObject*>(address() + kHeaderOffset) = object;
}

inline RawObject RawInstance::instanceVariableAt(word offset) const {
  DCHECK_INDEX(offset, headerCountOrOverflow() * kPointerSize);
  return *reinterpret_cast<RawObject*>(address() + offset);
}

inline void RawInstance::instanceVariableAtPut(word offset,
                                               RawObject value) const {
  DCHECK_INDEX(offset, headerCountOrOverflow() * kPointerSize);
  *reinterpret_cast<RawObject*>(address() + offset) = value;
}

inline void RawInstance::setLayoutId(LayoutId layout_id) const {
  setHeader(header().withLayoutId(layout_id));
}

// RawBaseException

inline RawObject RawBaseException::args() const {
  return instanceVariableAt(kArgsOffset);
}

inline void RawBaseException::setArgs(RawObject args) const {
  instanceVariableAtPut(kArgsOffset, args);
}

inline RawObject RawBaseException::traceback() const {
  RawObject o = tracebackOrUnbound();
  return o.isUnbound() ? RawNoneType::object() : o;
}

inline RawObject RawBaseException::tracebackOrUnbound() const {
  return instanceVariableAt(kTracebackOffset);
}

inline void RawBaseException::setTraceback(RawObject traceback) const {
  instanceVariableAtPut(kTracebackOffset, traceback);
}

inline RawObject RawBaseException::cause() const {
  RawObject o = causeOrUnbound();
  return o.isUnbound() ? RawNoneType::object() : o;
}

inline RawObject RawBaseException::causeOrUnbound() const {
  return instanceVariableAt(kCauseOffset);
}

inline void RawBaseException::setCause(RawObject cause) const {
  instanceVariableAtPut(kCauseOffset, cause);
}

inline RawObject RawBaseException::context() const {
  RawObject o = contextOrUnbound();
  return o.isUnbound() ? RawNoneType::object() : o;
}

inline RawObject RawBaseException::contextOrUnbound() const {
  return instanceVariableAt(kContextOffset);
}

inline void RawBaseException::setContext(RawObject context) const {
  instanceVariableAtPut(kContextOffset, context);
}

inline RawObject RawBaseException::suppressContext() const {
  return instanceVariableAt(kSuppressContextOffset);
}

inline void RawBaseException::setSuppressContext(RawObject suppress) const {
  instanceVariableAtPut(kSuppressContextOffset, suppress);
}
// RawStopIteration

inline RawObject RawStopIteration::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawStopIteration::setValue(RawObject value) const {
  instanceVariableAtPut(kValueOffset, value);
}

// RawSystemExit

inline RawObject RawSystemExit::code() const {
  return instanceVariableAt(kCodeOffset);
}

inline void RawSystemExit::setCode(RawObject code) const {
  instanceVariableAtPut(kCodeOffset, code);
}

// RawImportError

inline RawObject RawImportError::msg() const {
  return instanceVariableAt(kMsgOffset);
}

inline void RawImportError::setMsg(RawObject msg) const {
  instanceVariableAtPut(kMsgOffset, msg);
}

inline RawObject RawImportError::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawImportError::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

inline RawObject RawImportError::path() const {
  return instanceVariableAt(kPathOffset);
}

inline void RawImportError::setPath(RawObject path) const {
  instanceVariableAtPut(kPathOffset, path);
}

// RawAttributeDict

inline RawObject RawAttributeDict::attributes() const {
  return instanceVariableAt(kAttributesOffset);
}

inline void RawAttributeDict::setAttributes(RawObject mutable_tuple) const {
  instanceVariableAtPut(kAttributesOffset, mutable_tuple);
}

inline word RawAttributeDict::attributesRemaining() const {
  return RawSmallInt::cast(instanceVariableAt(kAttributesRemainingOffset))
      .value();
}

inline void RawAttributeDict::setAttributesRemaining(word free) const {
  instanceVariableAtPut(kAttributesRemainingOffset,
                        RawSmallInt::fromWord(free));
}

// RawType

inline RawObject RawType::bases() const {
  return instanceVariableAt(kBasesOffset);
}

inline void RawType::setBases(RawObject bases_tuple) const {
  instanceVariableAtPut(kBasesOffset, bases_tuple);
}

inline RawObject RawType::doc() const { return instanceVariableAt(kDocOffset); }

inline void RawType::setDoc(RawObject doc) const {
  instanceVariableAtPut(kDocOffset, doc);
}

inline RawObject RawType::mro() const { return instanceVariableAt(kMroOffset); }

inline void RawType::setMro(RawObject object_array) const {
  instanceVariableAtPut(kMroOffset, object_array);
}

inline RawObject RawType::instanceLayout() const {
  return instanceVariableAt(kInstanceLayoutOffset);
}

inline void RawType::setInstanceLayout(RawObject layout) const {
  instanceVariableAtPut(kInstanceLayoutOffset, layout);
}

inline LayoutId RawType::instanceLayoutId() const {
  return static_cast<LayoutId>(
      RawSmallInt::cast(instanceVariableAt(kInstanceLayoutIdOffset)).value());
}

inline void RawType::setInstanceLayoutId(LayoutId id) const {
  instanceVariableAtPut(kInstanceLayoutIdOffset,
                        RawSmallInt::fromWord(static_cast<word>(id)));
}

inline RawObject RawType::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawType::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

inline RawType::Flag RawType::flags() const {
  return static_cast<Flag>(
      RawSmallInt::cast(instanceVariableAt(kFlagsOffset)).value());
}

inline void RawType::setFlagsAndBuiltinBase(Flag value, LayoutId base) const {
  auto raw_base = static_cast<int>(base);
  DCHECK((raw_base & kBuiltinBaseMask) == raw_base,
         "Builtin base LayoutId too high");
  setFlags(static_cast<Flag>((value & ~kBuiltinBaseMask) | raw_base));
}

inline void RawType::setFlags(Flag value) const {
  instanceVariableAtPut(kFlagsOffset, RawSmallInt::fromWord(value));
}

inline void RawType::setBuiltinBase(LayoutId base) const {
  auto raw = static_cast<int>(base);
  DCHECK((raw & kBuiltinBaseMask) == raw, "Builtin base LayoutId too high");
  setFlags(static_cast<Flag>((flags() & ~kBuiltinBaseMask) | raw));
}

inline bool RawType::hasFlag(Flag bit) const { return (flags() & bit) != 0; }

inline LayoutId RawType::builtinBase() const {
  return static_cast<LayoutId>(flags() & kBuiltinBaseMask);
}

inline bool RawType::hasCustomDict() const {
  return hasFlag(RawType::Flag::kHasCustomDict);
}

inline bool RawType::hasNativeData() const {
  return hasFlag(RawType::Flag::kHasNativeData);
}

inline bool RawType::isCPythonHeaptype() const {
  return hasFlag(RawType::Flag::kIsCPythonHeaptype);
}

inline bool RawType::isBasetype() const {
  return hasFlag(RawType::Flag::kIsBasetype);
}

inline RawObject RawType::slots() const {
  return instanceVariableAt(kSlotsOffset);
}

inline void RawType::setSlots(RawObject slots) const {
  instanceVariableAtPut(kSlotsOffset, slots);
}

inline RawObject RawType::abstractMethods() const {
  return instanceVariableAt(kAbstractMethodsOffset);
}

inline void RawType::setAbstractMethods(RawObject methods) const {
  instanceVariableAtPut(kAbstractMethodsOffset, methods);
}

inline RawObject RawType::subclasses() const {
  return instanceVariableAt(kSubclassesOffset);
}

inline void RawType::setSubclasses(RawObject subclasses) const {
  instanceVariableAtPut(kSubclassesOffset, subclasses);
}

inline RawObject RawType::proxy() const {
  return instanceVariableAt(kProxyOffset);
}

inline void RawType::setProxy(RawObject proxy) const {
  instanceVariableAtPut(kProxyOffset, proxy);
}

inline RawObject RawType::ctor() const {
  return instanceVariableAt(kCtorOffset);
}

inline void RawType::setCtor(RawObject function) const {
  instanceVariableAtPut(kCtorOffset, function);
}

inline RawObject RawType::qualname() const {
  return instanceVariableAt(kQualnameOffset);
}

inline void RawType::setQualname(RawObject qualname) const {
  instanceVariableAtPut(kQualnameOffset, qualname);
}

inline bool RawType::isBuiltin() const {
  return instanceLayoutId() <= LayoutId::kLastBuiltinId;
}

inline bool RawType::isBaseExceptionSubclass() const {
  LayoutId base = builtinBase();
  return base >= LayoutId::kFirstException && base <= LayoutId::kLastException;
}

inline bool RawType::hasMutableDict() const { return isCPythonHeaptype(); }

// RawContext

inline RawObject RawContext::data() const {
  return instanceVariableAt(kDataOffset);
}

inline void RawContext::setData(RawObject data) const {
  instanceVariableAtPut(kDataOffset, data);
}

inline RawObject RawContext::prevContext() const {
  return instanceVariableAt(kPrevContextOffset);
}

inline void RawContext::setPrevContext(RawObject prev_context) const {
  instanceVariableAtPut(kPrevContextOffset, prev_context);
}

// RawContextVar

inline RawObject RawContextVar::defaultValue() const {
  return instanceVariableAt(kDefaultValueOffset);
}

inline void RawContextVar::setDefaultValue(RawObject default_value) const {
  instanceVariableAtPut(kDefaultValueOffset, default_value);
}

inline RawObject RawContextVar::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawContextVar::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

// RawTypeProxy

inline RawObject RawTypeProxy::type() const {
  return instanceVariableAt(kTypeOffset);
}

inline void RawTypeProxy::setType(RawObject type) const {
  instanceVariableAtPut(kTypeOffset, type);
}

// RawDataArray

inline word RawDataArray::allocationSize(word length) {
  DCHECK(length >= 0, "invalid length %ld", length);
  word size = headerSize(length) + length;
  return roundAllocationSize(size);
}

inline byte RawDataArray::byteAt(word index) const {
  DCHECK_INDEX(index, length());
  return *reinterpret_cast<byte*>(address() + index);
}

inline void RawDataArray::copyTo(byte* dst, word length) const {
  DCHECK_BOUND(length, this->length());
  copyToStartAt(dst, length, 0);
}

inline void RawDataArray::copyToStartAt(byte* dst, word length,
                                        word index) const {
  DCHECK_BOUND(index + length, this->length());
  std::memmove(dst, reinterpret_cast<const byte*>(address() + index), length);
}

inline word RawDataArray::length() const { return headerCountOrOverflow(); }

inline uint16_t RawDataArray::uint16At(word index) const {
  uint16_t result;
  DCHECK_INDEX(index, length() - static_cast<word>(sizeof(result) - 1));
  std::memcpy(&result, reinterpret_cast<const char*>(address() + index),
              sizeof(result));
  return result;
}

inline uint32_t RawDataArray::uint32At(word index) const {
  uint32_t result;
  DCHECK_INDEX(index, length() - static_cast<word>(sizeof(result) - 1));
  std::memcpy(&result, reinterpret_cast<const char*>(address() + index),
              sizeof(result));
  return result;
}

inline uint64_t RawDataArray::uint64At(word index) const {
  uint64_t result;
  DCHECK_INDEX(index, length() - static_cast<word>(sizeof(result) - 1));
  std::memcpy(&result, reinterpret_cast<const char*>(address() + index),
              sizeof(result));
  return result;
}

inline RawObject RawDataArray::initialize(uword address, word length,
                                          LayoutId layout_id) {
  return initializeHeader(address, /*count=*/length, /*hash=*/0, layout_id,
                          ObjectFormat::kData);
}

// RawLargeBytes

inline word RawLargeBytes::allocationSize(word length) {
  DCHECK(length > RawSmallBytes::kMaxLength, "length %ld is too small",
         (long)length);
  return RawDataArray::allocationSize(length);
}

// RawMutableBytes

inline word RawMutableBytes::allocationSize(word length) {
  return RawDataArray::allocationSize(length);
}

inline void RawMutableBytes::byteAtPut(word index, byte value) const {
  DCHECK_INDEX(index, length());
  *reinterpret_cast<byte*>(address() + index) = value;
}

inline void RawMutableBytes::uint16AtPut(word index, uint16_t value) const {
  DCHECK_INDEX(index, length() - static_cast<word>(sizeof(value) - 1));
  std::memcpy(reinterpret_cast<char*>(address() + index), &value,
              sizeof(value));
}

inline void RawMutableBytes::uint32AtPut(word index, uint32_t value) const {
  DCHECK_INDEX(index, length() - static_cast<word>(sizeof(value) - 1));
  std::memcpy(reinterpret_cast<char*>(address() + index), &value,
              sizeof(value));
}

// RawArray

inline RawObject RawArray::buffer() const {
  return instanceVariableAt(kBufferOffset);
}

inline void RawArray::setBuffer(RawObject new_buffer) const {
  DCHECK(new_buffer.isMutableBytes(), "Array must be backed by MutableBytes");
  instanceVariableAtPut(kBufferOffset, new_buffer);
}

inline word RawArray::length() const {
  return RawSmallInt::cast(instanceVariableAt(kLengthOffset)).value();
}

inline void RawArray::setLength(word new_length) const {
  instanceVariableAtPut(kLengthOffset, RawSmallInt::fromWord(new_length));
}

inline RawObject RawArray::typecode() const {
  return instanceVariableAt(kTypecodeOffset);
}

inline void RawArray::setTypecode(RawObject new_typecode) const {
  instanceVariableAtPut(kTypecodeOffset, new_typecode);
}

// RawMutableTuple

inline word RawMutableTuple::allocationSize(word length) {
  DCHECK(length >= 0, "invalid length %ld", length);
  word size = headerSize(length) + length * kPointerSize;
  return roundAllocationSize(size);
}

inline RawObject RawMutableTuple::becomeImmutable() const {
  setHeader(header().withLayoutId(LayoutId::kTuple));
  return *this;
}

inline void RawMutableTuple::swap(word i, word j) const {
  RawObject tmp = at(i);
  atPut(i, at(j));
  atPut(j, tmp);
}

inline RawObject RawMutableTuple::initialize(uword address, word length) {
  return initializeHeader(address, /*count=*/length, /*hash=*/0,
                          LayoutId::kMutableTuple, ObjectFormat::kObjects);
}

// RawTuple

inline word RawTuple::length() const { return headerCountOrOverflow(); }

inline RawObject RawTuple::at(word index) const {
  DCHECK_INDEX(index, length());
  return *reinterpret_cast<RawObject*>(address() + (index * kPointerSize));
}

inline void RawTuple::atPut(word index, RawObject value) const {
  DCHECK_INDEX(index, length());
  *reinterpret_cast<RawObject*>(address() + (index * kPointerSize)) = value;
}

// RawUserTupleBase

inline RawObject RawUserTupleBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserTupleBase::setValue(RawObject value) const {
  DCHECK(value.isTuple(), "Only tuple type is permitted as a value");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawTuple tupleUnderlying(RawObject object) {
  if (object.isTuple()) {
    return RawTuple::cast(object);
  }
  return RawTuple::cast(object.rawCast<RawUserTupleBase>().value());
}

// RawUnicodeError

inline RawObject RawUnicodeErrorBase::encoding() const {
  return instanceVariableAt(kEncodingOffset);
}

inline void RawUnicodeErrorBase::setEncoding(RawObject encoding_name) const {
  DCHECK(encoding_name.isStr(), "Only string type is permitted as a value");
  instanceVariableAtPut(kEncodingOffset, encoding_name);
}

inline RawObject RawUnicodeErrorBase::object() const {
  return instanceVariableAt(kObjectOffset);
}

inline void RawUnicodeErrorBase::setObject(RawObject value) const {
  DCHECK(value.isBytes() || value.isBytearray() || value.isStr(),
         "Only str or bytes-like types are permitted as values");
  instanceVariableAtPut(kObjectOffset, value);
}

inline RawObject RawUnicodeErrorBase::start() const {
  return instanceVariableAt(kStartOffset);
}

inline void RawUnicodeErrorBase::setStart(RawObject index) const {
  DCHECK(index.isInt(), "Only int type is permitted as a value");
  instanceVariableAtPut(kStartOffset, index);
}

inline RawObject RawUnicodeErrorBase::end() const {
  return instanceVariableAt(kEndOffset);
}

inline void RawUnicodeErrorBase::setEnd(RawObject index) const {
  DCHECK(index.isInt(), "Only int type is permitted as a value");
  instanceVariableAtPut(kEndOffset, index);
}

inline RawObject RawUnicodeErrorBase::reason() const {
  return instanceVariableAt(kReasonOffset);
}

inline void RawUnicodeErrorBase::setReason(RawObject error_description) const {
  DCHECK(error_description.isStr(), "Only string type is permitted as a value");
  instanceVariableAtPut(kReasonOffset, error_description);
}

// RawCode

inline word RawCode::argcount() const {
  return RawSmallInt::cast(instanceVariableAt(kArgcountOffset)).value();
}

inline void RawCode::setArgcount(word value) const {
  instanceVariableAtPut(kArgcountOffset, RawSmallInt::fromWord(value));
}

inline word RawCode::posonlyargcount() const {
  return RawSmallInt::cast(instanceVariableAt(kPosonlyargcountOffset)).value();
}

inline void RawCode::setPosonlyargcount(word value) const {
  instanceVariableAtPut(kPosonlyargcountOffset, RawSmallInt::fromWord(value));
}

inline RawObject RawCode::cell2arg() const {
  return instanceVariableAt(kCell2argOffset);
}

inline word RawCode::totalArgs() const {
  uword f = flags();
  word res = argcount() + kwonlyargcount();
  if (f & kVarargs) {
    res++;
  }
  if (f & kVarkeyargs) {
    res++;
  }
  return res;
}

inline void RawCode::setCell2arg(RawObject value) const {
  instanceVariableAtPut(kCell2argOffset, value);
}

inline RawObject RawCode::cellvars() const {
  return instanceVariableAt(kCellvarsOffset);
}

inline void RawCode::setCellvars(RawObject value) const {
  instanceVariableAtPut(kCellvarsOffset, value);
}

inline word RawCode::numCellvars() const {
  RawObject object = cellvars();
  DCHECK(object.isNoneType() || object.isTuple(), "not an object array");
  if (object.isNoneType()) {
    return 0;
  }
  return RawTuple::cast(object).length();
}

inline RawObject RawCode::code() const {
  return instanceVariableAt(kCodeOffset);
}

inline void RawCode::setCode(RawObject value) const {
  instanceVariableAtPut(kCodeOffset, value);
}

inline RawObject RawCode::consts() const {
  return instanceVariableAt(kConstsOffset);
}

inline void RawCode::setConsts(RawObject value) const {
  instanceVariableAtPut(kConstsOffset, value);
}

inline RawObject RawCode::filename() const {
  return instanceVariableAt(kFilenameOffset);
}

inline void RawCode::setFilename(RawObject value) const {
  instanceVariableAtPut(kFilenameOffset, value);
}

inline word RawCode::firstlineno() const {
  return RawSmallInt::cast(instanceVariableAt(kFirstlinenoOffset)).value();
}

inline void RawCode::setFirstlineno(word value) const {
  instanceVariableAtPut(kFirstlinenoOffset, RawSmallInt::fromWord(value));
}

inline word RawCode::flags() const {
  return RawSmallInt::cast(instanceVariableAt(kFlagsOffset)).value();
}

inline void RawCode::setFlags(word value) const {
  instanceVariableAtPut(kFlagsOffset, RawSmallInt::fromWord(value));
}

inline RawObject RawCode::freevars() const {
  return instanceVariableAt(kFreevarsOffset);
}

inline void RawCode::setFreevars(RawObject value) const {
  instanceVariableAtPut(kFreevarsOffset, value);
}

inline word RawCode::numFreevars() const {
  RawObject object = freevars();
  DCHECK(object.isNoneType() || object.isTuple(), "not an object array");
  if (object.isNoneType()) {
    return 0;
  }
  return RawTuple::cast(object).length();
}

inline word RawCode::kwonlyargcount() const {
  return RawSmallInt::cast(instanceVariableAt(kKwonlyargcountOffset)).value();
}

inline void RawCode::setKwonlyargcount(word value) const {
  instanceVariableAtPut(kKwonlyargcountOffset, RawSmallInt::fromWord(value));
}

inline RawObject RawCode::lnotab() const {
  return instanceVariableAt(kLnotabOffset);
}

inline void RawCode::setLnotab(RawObject value) const {
  instanceVariableAtPut(kLnotabOffset, value);
}

inline RawObject RawCode::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawCode::setName(RawObject value) const {
  instanceVariableAtPut(kNameOffset, value);
}

inline RawObject RawCode::names() const {
  return instanceVariableAt(kNamesOffset);
}

inline void RawCode::setNames(RawObject value) const {
  instanceVariableAtPut(kNamesOffset, value);
}

inline word RawCode::nlocals() const {
  return RawSmallInt::cast(instanceVariableAt(kNlocalsOffset)).value();
}

inline void RawCode::setNlocals(word value) const {
  instanceVariableAtPut(kNlocalsOffset, RawSmallInt::fromWord(value));
}

inline word RawCode::stacksize() const {
  return RawSmallInt::cast(instanceVariableAt(kStacksizeOffset)).value();
}

inline void RawCode::setStacksize(word value) const {
  instanceVariableAtPut(kStacksizeOffset, RawSmallInt::fromWord(value));
}

inline RawObject RawCode::varnames() const {
  return instanceVariableAt(kVarnamesOffset);
}

inline void RawCode::setVarnames(RawObject value) const {
  instanceVariableAtPut(kVarnamesOffset, value);
}

inline bool RawCode::isAsyncGenerator() const {
  return flags() & RawFunction::Flags::kAsyncGenerator;
}

inline bool RawCode::isGeneratorLike() const {
  return flags() &
         (Flags::kCoroutine | Flags::kGenerator | Flags::kAsyncGenerator);
}

inline bool RawCode::hasFreevarsOrCellvars() const {
  return !(flags() & Flags::kNofree);
}

inline bool RawCode::hasOptimizedAndNewlocals() const {
  return (flags() & (Flags::kOptimized | Flags::kNewlocals)) ==
         (Flags::kOptimized | Flags::kNewlocals);
}

inline bool RawCode::hasOptimizedOrNewlocals() const {
  return flags() & (Flags::kOptimized | Flags::kNewlocals);
}

inline bool RawCode::isNative() const { return code().isInt(); }

inline void* RawCode::intrinsic() const {
  return RawSmallInt::cast(instanceVariableAt(kIntrinsicOffset))
      .asAlignedCPtr();
}

inline void RawCode::setIntrinsic(void* fp) const {
  instanceVariableAtPut(kIntrinsicOffset, RawSmallInt::fromAlignedCPtr(fp));
}

// RawLargeInt

inline word RawLargeInt::asWord() const {
  DCHECK(numDigits() == 1, "RawLargeInt cannot fit in a word");
  return static_cast<word>(digitAt(0));
}

inline void* RawLargeInt::asCPtr() const {
  DCHECK(numDigits() == 1, "Large integer cannot fit in a pointer");
  DCHECK(isPositive(), "Cannot cast a negative value to a C pointer");
  return reinterpret_cast<void*>(asWord());
}

template <typename T>
if_signed_t<T, OptInt<T>> RawLargeInt::asInt() const {
  static_assert(sizeof(T) <= sizeof(word), "T must not be larger than word");

  if (numDigits() > 1) {
    auto const high_digit = static_cast<word>(digitAt(numDigits() - 1));
    return high_digit < 0 ? OptInt<T>::underflow() : OptInt<T>::overflow();
  }
  if (numDigits() == 1) {
    auto const value = asWord();
    if (value <= std::numeric_limits<T>::max() &&
        value >= std::numeric_limits<T>::min()) {
      return OptInt<T>::valid(value);
    }
  }
  return OptInt<T>::overflow();
}

template <typename T>
if_unsigned_t<T, OptInt<T>> RawLargeInt::asInt() const {
  static_assert(sizeof(T) <= sizeof(word), "T must not be larger than word");

  if (isNegative()) return OptInt<T>::underflow();
  if (static_cast<size_t>(bitLength()) > sizeof(T) * kBitsPerByte) {
    return OptInt<T>::overflow();
  }
  // No T accepted by this function needs more than one digit.
  return OptInt<T>::valid(digitAt(0));
}

inline bool RawLargeInt::isEven() const {
  word lowest_digit = digitAt(0);
  return (lowest_digit & 1) == 0;
}

inline bool RawLargeInt::isNegative() const {
  word highest_digit = digitAt(numDigits() - 1);
  return highest_digit < 0;
}

inline bool RawLargeInt::isPositive() const {
  word highest_digit = digitAt(numDigits() - 1);
  return highest_digit >= 0;
}

inline uword RawLargeInt::digitAt(word index) const {
  DCHECK_INDEX(index, numDigits());
  return reinterpret_cast<uword*>(address() + kValueOffset)[index];
}

inline void RawLargeInt::digitAtPut(word index, uword digit) const {
  DCHECK_INDEX(index, numDigits());
  reinterpret_cast<uword*>(address() + kValueOffset)[index] = digit;
}

inline word RawLargeInt::numDigits() const {
  return headerCountOrOverflow() / kWordSize;
}

inline word RawLargeInt::allocationSize(word num_digits) {
  word size = headerSize(num_digits * kWordSize) + num_digits * kWordSize;
  return roundAllocationSize(size);
}

inline RawObject RawLargeInt::initialize(uword address, word num_digits) {
  return initializeHeader(address, num_digits * kWordSize, 0,
                          LayoutId::kLargeInt, ObjectFormat::kData);
}

// RawFloat

inline word RawFloat::allocationSize() {
  return roundAllocationSize(RawHeader::kSize + kSize);
}

inline double RawFloat::value() const {
  return *reinterpret_cast<double*>(address() + kValueOffset);
}

inline RawObject RawFloat::initialize(uword address, double value) {
  RawHeapObject raw =
      initializeHeader(address, /*count=*/RawFloat::kSize,
                       /*hash=*/0, LayoutId::kFloat, ObjectFormat::kData);
  *reinterpret_cast<double*>(raw.address() + kValueOffset) = value;
  return raw;
}

// RawComplex

inline word RawComplex::allocationSize() {
  return roundAllocationSize(RawHeader::kSize + kSize);
}

inline double RawComplex::real() const {
  return *reinterpret_cast<double*>(address() + kRealOffset);
}

inline double RawComplex::imag() const {
  return *reinterpret_cast<double*>(address() + kImagOffset);
}

inline RawObject RawComplex::initialize(uword address, double real,
                                        double imag) {
  RawHeapObject raw =
      initializeHeader(address, /*count=*/RawComplex::kSize,
                       /*hash=*/0, LayoutId::kComplex, ObjectFormat::kData);
  *reinterpret_cast<double*>(raw.address() + kRealOffset) = real;
  *reinterpret_cast<double*>(raw.address() + kImagOffset) = imag;
  return raw;
}

// RawFrameProxy

inline RawObject RawFrameProxy::back() const {
  return instanceVariableAt(kBackOffset);
}

inline void RawFrameProxy::setBack(RawObject back) const {
  instanceVariableAtPut(kBackOffset, back);
}

inline RawObject RawFrameProxy::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawFrameProxy::setFunction(RawObject function) const {
  instanceVariableAtPut(kFunctionOffset, function);
}

inline RawObject RawFrameProxy::lasti() const {
  return instanceVariableAt(kLastiOffset);
}

inline void RawFrameProxy::setLasti(RawObject lasti) const {
  instanceVariableAtPut(kLastiOffset, lasti);
}

inline RawObject RawFrameProxy::locals() const {
  return instanceVariableAt(kLocalsOffset);
}

inline void RawFrameProxy::setLocals(RawObject locals) const {
  instanceVariableAtPut(kLocalsOffset, locals);
}

// RawUserBytesBase

inline RawObject RawUserBytesBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserBytesBase::setValue(RawObject value) const {
  DCHECK(value.isBytes(), "Only bytes type is permitted as a value.");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawBytes bytesUnderlying(RawObject object) {
  if (object.isBytes()) {
    return RawBytes::cast(object);
  }
  return RawBytes::cast(object.rawCast<RawUserBytesBase>().value());
}

// RawUserComplexBase

inline RawObject RawUserComplexBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserComplexBase::setValue(RawObject value) const {
  DCHECK(value.isComplex(), "Only complex type is permitted as a value.");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawComplex complexUnderlying(RawObject object) {
  if (object.isComplex()) {
    return RawComplex::cast(object);
  }
  return RawComplex::cast(object.rawCast<RawUserComplexBase>().value());
}

// RawUserFloatBase

inline RawObject RawUserFloatBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserFloatBase::setValue(RawObject value) const {
  DCHECK(value.isFloat(), "Only float type is permitted as a value");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawFloat floatUnderlying(RawObject object) {
  if (object.isFloat()) {
    return RawFloat::cast(object);
  }
  return RawFloat::cast(object.rawCast<RawUserFloatBase>().value());
}

// RawUserIntBase

inline RawObject RawUserIntBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserIntBase::setValue(RawObject value) const {
  DCHECK(value.isSmallInt() || value.isLargeInt(),
         "Only int types, not bool, are permitted as a value.");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawInt intUnderlying(RawObject object) {
  if (object.isInt()) {
    return RawInt::cast(object);
  }
  return RawInt::cast(object.rawCast<RawUserIntBase>().value());
}

// RawUserStrBase

inline RawObject RawUserStrBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserStrBase::setValue(RawObject value) const {
  DCHECK(value.isStr(), "Only str type is permitted as a value.");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawStr strUnderlying(RawObject object) {
  if (object.isStr()) {
    return RawStr::cast(object);
  }
  return RawStr::cast(object.rawCast<RawUserStrBase>().value());
}

// RawRange

inline RawObject RawRange::start() const {
  return instanceVariableAt(kStartOffset);
}

inline void RawRange::setStart(RawObject value) const {
  instanceVariableAtPut(kStartOffset, value);
}

inline RawObject RawRange::stop() const {
  return instanceVariableAt(kStopOffset);
}

inline void RawRange::setStop(RawObject value) const {
  instanceVariableAtPut(kStopOffset, value);
}

inline RawObject RawRange::step() const {
  return instanceVariableAt(kStepOffset);
}

inline void RawRange::setStep(RawObject value) const {
  instanceVariableAtPut(kStepOffset, value);
}

// RawPointer

inline word RawPointer::allocationSize() {
  return roundAllocationSize(RawHeader::kSize + kSize);
}

inline void* RawPointer::cptr() const {
  return *reinterpret_cast<void**>(address() + kCPtrOffset);
}

inline void RawPointer::setCPtr(void* new_cptr) const {
  *reinterpret_cast<void**>(address() + kCPtrOffset) = new_cptr;
}

inline word RawPointer::length() const {
  return *reinterpret_cast<word*>(address() + kLengthOffset);
}

inline void RawPointer::setLength(word new_length) const {
  *reinterpret_cast<word*>(address() + kLengthOffset) = new_length;
}

inline RawObject RawPointer::initialize(uword address, void* cptr,
                                        word length) {
  RawHeapObject raw =
      initializeHeader(address, /*count=*/RawPointer::kSize,
                       /*hash=*/0, LayoutId::kPointer, ObjectFormat::kData);
  *reinterpret_cast<void**>(raw.address() + kCPtrOffset) = cptr;
  *reinterpret_cast<word*>(raw.address() + kLengthOffset) = length;
  return raw;
}

// RawProperty

inline RawObject RawProperty::getter() const {
  return instanceVariableAt(kGetterOffset);
}

inline void RawProperty::setGetter(RawObject function) const {
  instanceVariableAtPut(kGetterOffset, function);
}

inline RawObject RawProperty::setter() const {
  return instanceVariableAt(kSetterOffset);
}

inline void RawProperty::setSetter(RawObject function) const {
  instanceVariableAtPut(kSetterOffset, function);
}

inline RawObject RawProperty::deleter() const {
  return instanceVariableAt(kDeleterOffset);
}

inline void RawProperty::setDeleter(RawObject function) const {
  instanceVariableAtPut(kDeleterOffset, function);
}

// RawSlice

inline RawObject RawSlice::start() const {
  return instanceVariableAt(kStartOffset);
}

inline void RawSlice::setStart(RawObject value) const {
  instanceVariableAtPut(kStartOffset, value);
}

inline RawObject RawSlice::stop() const {
  return instanceVariableAt(kStopOffset);
}

inline void RawSlice::setStop(RawObject value) const {
  instanceVariableAtPut(kStopOffset, value);
}

inline RawObject RawSlice::step() const {
  return instanceVariableAt(kStepOffset);
}

inline void RawSlice::setStep(RawObject value) const {
  instanceVariableAtPut(kStepOffset, value);
}

// RawSlotDescriptor

inline RawObject RawSlotDescriptor::type() const {
  return RawSlotDescriptor::instanceVariableAt(kTypeOffset);
}

inline void RawSlotDescriptor::setType(RawObject type) const {
  instanceVariableAtPut(kTypeOffset, type);
}

inline RawObject RawSlotDescriptor::name() const {
  return RawSlotDescriptor::instanceVariableAt(kNameOffset);
}

inline void RawSlotDescriptor::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

inline word RawSlotDescriptor::offset() const {
  return RawSmallInt::cast(RawSlotDescriptor::instanceVariableAt(kOffsetOffset))
      .value();
}

inline void RawSlotDescriptor::setOffset(word offset) const {
  instanceVariableAtPut(kOffsetOffset, RawSmallInt::fromWord(offset));
}

// RawStaticMethod

inline RawObject RawStaticMethod::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawStaticMethod::setFunction(RawObject function) const {
  instanceVariableAtPut(kFunctionOffset, function);
}

// RawBytearray

inline byte RawBytearray::byteAt(word index) const {
  DCHECK_INDEX(index, numItems());
  return RawMutableBytes::cast(items()).byteAt(index);
}

inline void RawBytearray::byteAtPut(word index, byte value) const {
  DCHECK_INDEX(index, numItems());
  RawMutableBytes::cast(items()).byteAtPut(index, value);
}

inline void RawBytearray::copyTo(byte* dst, word length) const {
  DCHECK_BOUND(length, numItems());
  RawMutableBytes::cast(items()).copyTo(dst, length);
}

inline word RawBytearray::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawBytearray::setNumItems(word num_bytes) const {
  DCHECK_BOUND(num_bytes, capacity());
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_bytes));
}

inline RawObject RawBytearray::items() const {
  return instanceVariableAt(kItemsOffset);
}

inline void RawBytearray::setItems(RawObject new_items) const {
  DCHECK(new_items.isMutableBytes(), "backed by mutable bytes");
  instanceVariableAtPut(kItemsOffset, new_items);
}

inline word RawBytearray::capacity() const {
  return RawMutableBytes::cast(items()).length();
}

// RawStrArray

inline RawObject RawStrArray::items() const {
  return instanceVariableAt(kItemsOffset);
}

inline void RawStrArray::setItems(RawObject new_items) const {
  DCHECK(new_items.isMutableBytes(), "StrArray must be backed by MutableBytes");
  instanceVariableAtPut(kItemsOffset, new_items);
}

inline word RawStrArray::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawStrArray::setNumItems(word num_items) const {
  DCHECK_BOUND(num_items, capacity());
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline void RawStrArray::copyTo(byte* dst, word length) const {
  DCHECK_BOUND(length, numItems());
  RawMutableBytes::cast(items()).copyTo(dst, length);
}

inline word RawStrArray::capacity() const {
  return RawMutableBytes::cast(items()).length();
}

// RawDeque

inline RawObject RawDeque::at(word index) const {
  DCHECK_INDEX(index, capacity());
  return RawTuple::cast(items()).at(index);
}

inline void RawDeque::atPut(word index, RawObject value) const {
  DCHECK_INDEX(index, capacity());
  RawTuple::cast(items()).atPut(index, value);
}

inline word RawDeque::capacity() const {
  RawObject raw_items = items();
  if (raw_items == RawSmallInt::fromWord(0)) return 0;
  return RawTuple::cast(raw_items).length();
}

inline void RawDeque::clear() const {
  if (numItems() == 0) return;
  RawMutableTuple::cast(items()).fill(RawNoneType::object());
  setLeft(0);
  setNumItems(0);
}

inline RawObject RawDeque::items() const {
  return instanceVariableAt(kItemsOffset);
}

inline void RawDeque::setItems(RawObject new_items) const {
  instanceVariableAtPut(kItemsOffset, new_items);
}

inline word RawDeque::left() const {
  return RawSmallInt::cast(instanceVariableAt(kLeftOffset)).value();
}

inline void RawDeque::setLeft(word left) const {
  instanceVariableAtPut(kLeftOffset, RawSmallInt::fromWord(left));
}

inline word RawDeque::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawDeque::setNumItems(word num_items) const {
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline RawObject RawDeque::maxlen() const {
  return instanceVariableAt(kMaxlenOffset);
}

inline void RawDeque::setMaxlen(RawObject maxlen) const {
  instanceVariableAtPut(kMaxlenOffset, maxlen);
}

inline word RawDeque::state() const {
  return RawSmallInt::cast(instanceVariableAt(kStateOffset)).value();
}

inline void RawDeque::setState(word state) const {
  instanceVariableAtPut(kStateOffset, RawSmallInt::fromWord(state));
}

// RawDequeIterator

inline word RawDequeIterator::state() const {
  return RawSmallInt::cast(instanceVariableAt(kStateOffset)).value();
}

inline void RawDequeIterator::setState(word state) const {
  instanceVariableAtPut(kStateOffset, RawSmallInt::fromWord(state));
}

// RawDequeReverseIterator

inline word RawDequeReverseIterator::state() const {
  return RawSmallInt::cast(instanceVariableAt(kStateOffset)).value();
}

inline void RawDequeReverseIterator::setState(word state) const {
  instanceVariableAtPut(kStateOffset, RawSmallInt::fromWord(state));
}

// RawDict

inline word RawDict::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawDict::setNumItems(word num_items) const {
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline RawObject RawDict::data() const {
  return instanceVariableAt(kDataOffset);
}

inline void RawDict::setData(RawObject data) const {
  instanceVariableAtPut(kDataOffset, data);
}

inline RawObject RawDict::indices() const {
  return instanceVariableAt(kIndicesOffset);
}

inline void RawDict::setIndices(RawObject index_data) const {
  instanceVariableAtPut(kIndicesOffset, index_data);
}

inline word RawDict::firstEmptyItemIndex() const {
  return RawSmallInt::cast(instanceVariableAt(kFirstEmptyItemIndexOffset))
      .value();
}

inline void RawDict::setFirstEmptyItemIndex(word first_empty_item_index) const {
  instanceVariableAtPut(kFirstEmptyItemIndexOffset,
                        RawSmallInt::fromWord(first_empty_item_index));
}

inline word RawDict::numIndices() const {
  RawObject indices_obj = indices();
  if (indices_obj == RawSmallInt::fromWord(0)) return 0;
  return RawMutableBytes::cast(indices_obj).length() >> 2;
}

// RawDictIteratorBase

inline word RawDictIteratorBase::numFound() const {
  return RawSmallInt::cast(instanceVariableAt(kNumFoundOffset)).value();
}

inline void RawDictIteratorBase::setNumFound(word num_found) const {
  instanceVariableAtPut(kNumFoundOffset, RawSmallInt::fromWord(num_found));
}

// RawDictViewBase

inline RawObject RawDictViewBase::dict() const {
  return instanceVariableAt(kDictOffset);
}

inline void RawDictViewBase::setDict(RawObject dict) const {
  instanceVariableAtPut(kDictOffset, dict);
}

// RawFunction

inline RawObject RawFunction::annotations() const {
  return instanceVariableAt(kAnnotationsOffset);
}

inline void RawFunction::setAnnotations(RawObject annotations) const {
  instanceVariableAtPut(kAnnotationsOffset, annotations);
}

inline word RawFunction::argcount() const {
  return RawSmallInt::cast(instanceVariableAt(kArgcountOffset)).value();
}

inline void RawFunction::setArgcount(word value) const {
  instanceVariableAtPut(kArgcountOffset, RawSmallInt::fromWord(value));
}

inline RawObject RawFunction::closure() const {
  return instanceVariableAt(kClosureOffset);
}

inline void RawFunction::setClosure(RawObject closure) const {
  instanceVariableAtPut(kClosureOffset, closure);
}

inline RawObject RawFunction::code() const {
  return instanceVariableAt(kCodeOffset);
}

inline void RawFunction::setCode(RawObject code) const {
  instanceVariableAtPut(kCodeOffset, code);
}

inline RawObject RawFunction::defaults() const {
  return instanceVariableAt(kDefaultsOffset);
}

inline void RawFunction::setDefaults(RawObject defaults) const {
  instanceVariableAtPut(kDefaultsOffset, defaults);
}

inline bool RawFunction::hasDefaults() const {
  return !defaults().isNoneType();
}

inline RawObject RawFunction::doc() const {
  return instanceVariableAt(kDocOffset);
}

inline void RawFunction::setDoc(RawObject doc) const {
  instanceVariableAtPut(kDocOffset, doc);
}

inline RawFunction::Entry RawFunction::entry() const {
  RawObject object = instanceVariableAt(kEntryOffset);
  return reinterpret_cast<Entry>(RawSmallInt::cast(object).asAlignedCPtr());
}

inline void RawFunction::setEntry(RawFunction::Entry thunk) const {
  RawObject object =
      RawSmallInt::fromAlignedCPtr(reinterpret_cast<void*>(thunk));
  instanceVariableAtPut(kEntryOffset, object);
}

inline RawFunction::Entry RawFunction::entryKw() const {
  RawObject object = instanceVariableAt(kEntryKwOffset);
  return reinterpret_cast<Entry>(RawSmallInt::cast(object).asAlignedCPtr());
}

inline void RawFunction::setEntryKw(RawFunction::Entry thunk) const {
  RawObject object =
      RawSmallInt::fromAlignedCPtr(reinterpret_cast<void*>(thunk));
  instanceVariableAtPut(kEntryKwOffset, object);
}

inline RawFunction::Entry RawFunction::entryEx() const {
  RawObject object = instanceVariableAt(kEntryExOffset);
  return reinterpret_cast<Entry>(RawSmallInt::cast(object).asAlignedCPtr());
}

inline void RawFunction::setEntryEx(RawFunction::Entry thunk) const {
  RawObject object =
      RawSmallInt::fromAlignedCPtr(reinterpret_cast<void*>(thunk));
  instanceVariableAtPut(kEntryExOffset, object);
}

inline void* RawFunction::entryAsm() const {
  RawObject object = instanceVariableAt(kEntryAsmOffset);
  return reinterpret_cast<void*>(RawSmallInt::cast(object).asAlignedCPtr());
}

inline void RawFunction::setEntryAsm(void* thunk) const {
  RawObject object = RawSmallInt::fromAlignedCPtr(thunk);
  instanceVariableAtPut(kEntryAsmOffset, object);
}

inline word RawFunction::flags() const {
  return RawSmallInt::cast(instanceVariableAt(kFlagsOffset)).value();
}

inline void RawFunction::setFlags(word flags) const {
  instanceVariableAtPut(kFlagsOffset, RawSmallInt::fromWord(flags));
}

inline bool RawFunction::isAsyncGenerator() const {
  return flags() & Flags::kAsyncGenerator;
}

inline bool RawFunction::isCoroutine() const {
  return flags() & Flags::kCoroutine;
}

inline bool RawFunction::isExtension() const {
  return flags() & Flags::kExtension;
}

inline bool RawFunction::isCompiled() const {
  return flags() & Flags::kCompiled;
}

inline bool RawFunction::isGeneratorLike() const {
  return flags() &
         (Flags::kCoroutine | Flags::kGenerator | Flags::kAsyncGenerator);
}

inline bool RawFunction::hasFreevarsOrCellvars() const {
  return !(flags() & Flags::kNofree);
}

inline bool RawFunction::isGenerator() const {
  return flags() & Flags::kGenerator;
}

inline bool RawFunction::isIterableCoroutine() const {
  return flags() & Flags::kIterableCoroutine;
}

inline bool RawFunction::hasOptimizedOrNewlocals() const {
  return flags() & (Flags::kOptimized | Flags::kNewlocals);
}

inline bool RawFunction::hasSimpleCall() const {
  return flags() & Flags::kSimpleCall;
}

inline bool RawFunction::hasVarargs() const {
  return flags() & Flags::kVarargs;
}

inline bool RawFunction::hasVarkeyargs() const {
  return flags() & Flags::kVarkeyargs;
}

inline bool RawFunction::hasVarargsOrVarkeyargs() const {
  return flags() & (Flags::kVarargs | Flags::kVarkeyargs);
}

inline bool RawFunction::isInterpreted() const {
  return flags() & Flags::kInterpreted;
}

inline void RawFunction::setIsInterpreted(bool interpreted) const {
  setFlags(interpreted ? flags() | Flags::kInterpreted
                       : flags() & ~Flags::kInterpreted);
}

inline void* RawFunction::intrinsic() const {
  return RawSmallInt::cast(instanceVariableAt(kIntrinsicOffset))
      .asAlignedCPtr();
}

inline void RawFunction::setIntrinsic(void* fp) const {
  instanceVariableAtPut(kIntrinsicOffset, RawSmallInt::fromAlignedCPtr(fp));
}

inline RawObject RawFunction::kwDefaults() const {
  return instanceVariableAt(kKwDefaultsOffset);
}

inline void RawFunction::setKwDefaults(RawObject kw_defaults) const {
  instanceVariableAtPut(kKwDefaultsOffset, kw_defaults);
}

inline RawObject RawFunction::moduleName() const {
  return instanceVariableAt(kModuleNameOffset);
}

inline void RawFunction::setModuleName(RawObject module_name) const {
  DCHECK(module_name.isStr(), "module_name is expected to be a Str");
  instanceVariableAtPut(kModuleNameOffset, module_name);
}

inline RawObject RawFunction::moduleObject() const {
  return instanceVariableAt(kModuleObjectOffset);
}

inline void RawFunction::setModuleObject(RawObject module_object) const {
  instanceVariableAtPut(kModuleObjectOffset, module_object);
}

inline RawObject RawFunction::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawFunction::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

inline RawObject RawFunction::qualname() const {
  return instanceVariableAt(kQualnameOffset);
}

inline void RawFunction::setQualname(RawObject qualname) const {
  instanceVariableAtPut(kQualnameOffset, qualname);
}

inline word RawFunction::totalArgs() const {
  return RawSmallInt::cast(instanceVariableAt(kTotalArgsOffset)).value();
}

inline void RawFunction::setTotalArgs(word value) const {
  instanceVariableAtPut(kTotalArgsOffset, RawSmallInt::fromWord(value));
}

inline word RawFunction::totalVars() const {
  return RawSmallInt::cast(instanceVariableAt(kTotalVarsOffset)).value();
}

inline void RawFunction::setTotalVars(word value) const {
  instanceVariableAtPut(kTotalVarsOffset, RawSmallInt::fromWord(value));
}

inline word RawFunction::totalLocals() const {
  return totalArgs() + totalVars();
}

inline RawObject RawFunction::stacksizeOrBuiltin() const {
  return instanceVariableAt(kStacksizeOrBuiltinOffset);
}

inline void RawFunction::setStacksizeOrBuiltin(
    RawObject stacksize_or_builtin) const {
  instanceVariableAtPut(kStacksizeOrBuiltinOffset, stacksize_or_builtin);
}

inline RawObject RawFunction::rewrittenBytecode() const {
  return instanceVariableAt(kRewrittenBytecodeOffset);
}

inline void RawFunction::setRewrittenBytecode(
    RawObject rewritten_bytecode) const {
  instanceVariableAtPut(kRewrittenBytecodeOffset, rewritten_bytecode);
}

inline RawObject RawFunction::caches() const {
  return instanceVariableAt(kCachesOffset);
}

inline void RawFunction::setCaches(RawObject cache) const {
  instanceVariableAtPut(kCachesOffset, cache);
}

inline RawObject RawFunction::dict() const {
  return instanceVariableAt(kDictOffset);
}

inline void RawFunction::setDict(RawObject dict) const {
  instanceVariableAtPut(kDictOffset, dict);
}

// RawInstance

inline word RawInstance::allocationSize(word num_attr) {
  DCHECK(num_attr >= 0, "invalid number of attributes %ld", num_attr);
  word size = headerSize(num_attr) + num_attr * kPointerSize;
  return roundAllocationSize(size);
}

// RawList

inline RawObject RawList::items() const {
  return instanceVariableAt(kItemsOffset);
}

inline void RawList::setItems(RawObject new_items) const {
  instanceVariableAtPut(kItemsOffset, new_items);
}

inline word RawList::capacity() const {
  return RawTuple::cast(items()).length();
}

inline word RawList::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawList::setNumItems(word num_items) const {
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline void RawList::clearFrom(word idx) const {
  if (numItems() == 0) return;
  DCHECK_INDEX(idx, numItems());
  std::memset(reinterpret_cast<byte*>(RawTuple::cast(items()).address()) +
                  idx * kPointerSize,
              -1, (numItems() - idx) * kWordSize);
  setNumItems(idx);
}

inline void RawList::atPut(word index, RawObject value) const {
  DCHECK_INDEX(index, numItems());
  RawObject items = instanceVariableAt(kItemsOffset);
  RawTuple::cast(items).atPut(index, value);
}

inline RawObject RawList::at(word index) const {
  DCHECK_INDEX(index, numItems());
  return RawTuple::cast(items()).at(index);
}

inline void RawList::swap(word i, word j) const {
  DCHECK_INDEX(i, numItems());
  DCHECK_INDEX(j, numItems());
  RawMutableTuple::cast(items()).swap(i, j);
}

// RawMappingProxy

inline RawObject RawMappingProxy::mapping() const {
  return RawMappingProxy::instanceVariableAt(kMappingOffset);
}

inline void RawMappingProxy::setMapping(RawObject mapping) const {
  instanceVariableAtPut(kMappingOffset, mapping);
}

// RawMemoryView

inline RawObject RawMemoryView::buffer() const {
  return RawMemoryView::instanceVariableAt(kBufferOffset);
}

inline void RawMemoryView::setBuffer(RawObject buffer) const {
  instanceVariableAtPut(kBufferOffset, buffer);
}

inline RawObject RawMemoryView::format() const {
  return RawMemoryView::instanceVariableAt(kFormatOffset);
}

inline void RawMemoryView::setFormat(RawObject format) const {
  instanceVariableAtPut(kFormatOffset, format);
}

inline bool RawMemoryView::readOnly() const {
  return RawBool::cast(instanceVariableAt(kReadOnlyOffset)).value();
}

inline void RawMemoryView::setReadOnly(bool read_only) const {
  instanceVariableAtPut(kReadOnlyOffset, RawBool::fromBool(read_only));
}

inline word RawMemoryView::length() const {
  return RawSmallInt::cast(instanceVariableAt(kLengthOffset)).value();
}

inline void RawMemoryView::setLength(word length) const {
  instanceVariableAtPut(kLengthOffset, RawSmallInt::fromWord(length));
}

inline RawObject RawMemoryView::object() const {
  return RawMemoryView::instanceVariableAt(kObjectOffset);
}

inline void RawMemoryView::setObject(RawObject object) const {
  instanceVariableAtPut(kObjectOffset, object);
}

inline RawObject RawMemoryView::ndim() const {
  return RawMemoryView::instanceVariableAt(kNdimOffset);
}

inline void RawMemoryView::setNdim(RawObject ndim) const {
  instanceVariableAtPut(kNdimOffset, ndim);
}

inline RawObject RawMemoryView::shape() const {
  return RawMemoryView::instanceVariableAt(kShapeOffset);
}

inline void RawMemoryView::setShape(RawObject shape) const {
  instanceVariableAtPut(kShapeOffset, shape);
}

inline word RawMemoryView::start() const {
  return RawSmallInt::cast(instanceVariableAt(kStartOffset)).value();
}

inline void RawMemoryView::setStart(word start) const {
  instanceVariableAtPut(kStartOffset, RawSmallInt::fromWord(start));
}

inline RawObject RawMemoryView::strides() const {
  return RawMemoryView::instanceVariableAt(kStridesOffset);
}

inline void RawMemoryView::setStrides(RawObject strides) const {
  instanceVariableAtPut(kStridesOffset, strides);
}

// RawMmap

inline word RawMmap::access() const {
  return RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value();
}

inline void RawMmap::setAccess(word new_access) const {
  instanceVariableAtPut(kAccessOffset, RawSmallInt::fromWord(new_access));
}

inline RawObject RawMmap::data() const {
  return instanceVariableAt(kDataOffset);
}

inline void RawMmap::setData(RawObject new_data) const {
  instanceVariableAtPut(kDataOffset, new_data);
}

inline RawObject RawMmap::fd() const { return instanceVariableAt(kFdOffset); }

inline void RawMmap::setFd(RawObject new_fd) const {
  instanceVariableAtPut(kFdOffset, new_fd);
}

inline bool RawMmap::isReadable() const {
  return RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value() &
         Property::kReadable;
}

inline void RawMmap::setReadable() const {
  word mask = RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value();
  instanceVariableAtPut(kAccessOffset,
                        RawSmallInt::fromWord(mask | Property::kReadable));
}

inline bool RawMmap::isWritable() const {
  return RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value() &
         Property::kWritable;
}

inline void RawMmap::setWritable() const {
  word mask = RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value();
  instanceVariableAtPut(kAccessOffset,
                        RawSmallInt::fromWord(mask | Property::kWritable));
}

inline bool RawMmap::isCopyOnWrite() const {
  return RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value() &
         Property::kCopyOnWrite;
}

inline void RawMmap::setCopyOnWrite() const {
  word mask = RawSmallInt::cast(instanceVariableAt(kAccessOffset)).value();
  instanceVariableAtPut(kAccessOffset,
                        RawSmallInt::fromWord(mask | Property::kCopyOnWrite));
}

// RawModule

inline RawObject RawModule::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawModule::setName(RawObject name) const {
  instanceVariableAtPut(kNameOffset, name);
}

inline RawObject RawModule::def() const {
  return instanceVariableAt(kDefOffset);
}

inline bool RawModule::hasDef() const {
  RawObject def_value = def();
  return def_value.isInt() && RawInt::cast(def_value).asCPtr() != nullptr;
}

inline void RawModule::setDef(RawObject def) const {
  instanceVariableAtPut(kDefOffset, def);
}

inline RawObject RawModule::state() const {
  return instanceVariableAt(kStateOffset);
}

inline bool RawModule::hasState() const {
  RawObject state_value = state();
  return state_value.isInt() && RawInt::cast(state_value).asCPtr() != nullptr;
}

inline void RawModule::setState(RawObject state) const {
  instanceVariableAtPut(kStateOffset, state);
}

inline RawObject RawModule::moduleProxy() const {
  return instanceVariableAt(kModuleProxyOffset);
}

inline void RawModule::setModuleProxy(RawObject module_proxy) const {
  instanceVariableAtPut(kModuleProxyOffset, module_proxy);
}

inline word RawModule::id() const {
  word index = header().hashCode();
  DCHECK(index != RawHeader::kUninitializedHash,
         "Module header hash field should contain a valid ID");
  return index;
}

inline void RawModule::setId(word id) const {
  DCHECK(static_cast<word>(id & RawHeader::kHashCodeMask) == id,
         "Module ID %ld doesn't fit in hash code", id);
  setHeader(header().withHashCode(id));
}

inline bool RawModule::isBuiltin() const {
  return id() & kBuiltinTagMask;
}

// RawModuleProxy

inline RawObject RawModuleProxy::module() const {
  return instanceVariableAt(kModuleOffset);
}

inline void RawModuleProxy::setModule(RawObject module) const {
  instanceVariableAtPut(kModuleOffset, module);
}

// RawStr

inline byte RawStr::byteAt(word index) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).byteAt(index);
  }
  return RawLargeStr::cast(*this).byteAt(index);
}

inline int32_t RawStr::codePointAt(word index, word* char_length) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).codePointAt(index, char_length);
  }
  return RawLargeStr::cast(*this).codePointAt(index, char_length);
}

inline word RawStr::length() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).length();
  }
  return RawLargeStr::cast(*this).length();
}

inline word RawStr::compare(RawStr that) const {
  if (*this == that) {
    return 0;
  }
  if (isImmediateObjectNotSmallInt()) {
    if (that.isSmallStr()) {
      word result = __builtin_bswap64(this->raw() & ~uword{0xFF}) -
                    __builtin_bswap64(that.raw() & ~uword{0xFF});
      return LIKELY(result != 0)
                 ? result
                 : this->length() - RawSmallStr::cast(that).length();
    }
    return RawSmallStr::cast(*this).compare(that);
  }
  if (that.isImmediateObjectNotSmallInt()) {
    return -RawSmallStr::cast(that).compare(*this);
  }
  return RawLargeStr::cast(*this).compare(RawLargeStr::cast(that));
}

inline void RawStr::copyTo(byte* dst, word length) const {
  if (isImmediateObjectNotSmallInt()) {
    RawSmallStr::cast(*this).copyTo(dst, length);
    return;
  }
  return RawLargeStr::cast(*this).copyTo(dst, length);
}

inline void RawStr::copyToStartAt(byte* dst, word char_length,
                                  word char_start) const {
  if (isImmediateObjectNotSmallInt()) {
    RawSmallStr::cast(*this).copyToStartAt(dst, char_length, char_start);
    return;
  }
  return RawLargeStr::cast(*this).copyToStartAt(dst, char_length, char_start);
}

inline word RawStr::codePointLength() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).codePointLength();
  }
  return RawLargeStr::cast(*this).codePointLength();
}

inline RawStr RawStr::empty() { return RawSmallStr::empty().rawCast<RawStr>(); }

inline bool RawStr::equals(RawStr that) const {
  if (*this == that) return true;
  if (isImmediateObjectNotSmallInt()) return false;
  if (that.isImmediateObjectNotSmallInt()) return false;
  return RawLargeStr::cast(*this).equals(RawLargeStr::cast(that));
}

inline bool RawStr::equalsCStr(const char* c_str) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).equalsCStr(c_str);
  }
  return RawLargeStr::cast(*this).equalsCStr(c_str);
}

inline bool RawStr::includes(RawObject that) const {
  if (*this == that) return true;
  if (isSmallStr()) {
    return RawSmallStr::cast(*this).includes(that);
  }
  return RawLargeStr::cast(*this).includes(that);
}

inline bool RawStr::includesByte(byte b) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).includesByte(b);
  }
  return RawLargeStr::cast(*this).includesByte(b);
}

inline bool RawStr::isASCII() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).isASCII();
  }
  return RawLargeStr::cast(*this).isASCII();
}

inline word RawStr::occurrencesOf(RawObject that) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).occurrencesOf(that);
  }
  return RawLargeStr::cast(*this).occurrencesOf(that);
}

inline word RawStr::offsetByCodePoints(word index, word count) const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).offsetByCodePoints(index, count);
  }
  return RawLargeStr::cast(*this).offsetByCodePoints(index, count);
}

inline char* RawStr::toCStr() const {
  if (isImmediateObjectNotSmallInt()) {
    return RawSmallStr::cast(*this).toCStr();
  }
  return RawLargeStr::cast(*this).toCStr();
}

// RawLargeStr

inline word RawLargeStr::allocationSize(word length) {
  DCHECK(length > RawSmallStr::kMaxLength, "length %ld is too small",
         (long)length);
  return RawDataArray::allocationSize(length);
}

// RawValueCell

inline RawObject RawValueCell::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawValueCell::setValue(RawObject object) const {
  // TODO(T44801497): Disallow a ValueCell in another ValueCell.
  DCHECK(*this != object, "ValueCell can't self-reference itself");
  instanceVariableAtPut(kValueOffset, object);
}

inline RawObject RawValueCell::dependencyLink() const {
  return instanceVariableAt(kDependencyLinkOffset);
}

inline void RawValueCell::setDependencyLink(RawObject object) const {
  instanceVariableAtPut(kDependencyLinkOffset, object);
}

inline void RawValueCell::makePlaceholder() const {
  instanceVariableAtPut(kValueOffset, *this);
}

inline bool RawValueCell::isPlaceholder() const { return *this == value(); }

// RawEllipsis

inline word RawEllipsis::allocationSize() {
  return roundAllocationSize(RawHeader::kSize);
}

inline RawObject RawEllipsis::initialize(uword address) {
  return initializeHeader(address, /*count=*/0,
                          /*hash=*/0, LayoutId::kEllipsis, ObjectFormat::kData);
}

// RawSetBase

inline word RawSetBase::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawSetBase::setNumItems(word num_items) const {
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline word RawSetBase::numFilled() const {
  return RawSmallInt::cast(instanceVariableAt(kNumFilledOffset)).value();
}

inline void RawSetBase::setNumFilled(word num_filled) const {
  instanceVariableAtPut(kNumFilledOffset, RawSmallInt::fromWord(num_filled));
}

inline RawObject RawSetBase::data() const {
  return instanceVariableAt(kDataOffset);
}

inline void RawSetBase::setData(RawObject data) const {
  instanceVariableAtPut(kDataOffset, data);
}

// RawBoundMethod

inline RawObject RawBoundMethod::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawBoundMethod::setFunction(RawObject function) const {
  instanceVariableAtPut(kFunctionOffset, function);
}

inline RawObject RawBoundMethod::self() const {
  return instanceVariableAt(kSelfOffset);
}

inline void RawBoundMethod::setSelf(RawObject self) const {
  instanceVariableAtPut(kSelfOffset, self);
}

// RawCell

inline RawObject RawCell::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawCell::setValue(RawObject value) const {
  instanceVariableAtPut(kValueOffset, value);
}

// RawClassMethod

inline RawObject RawClassMethod::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawClassMethod::setFunction(RawObject function) const {
  instanceVariableAtPut(kFunctionOffset, function);
}

// RawToken

inline RawObject RawToken::context() const {
  return instanceVariableAt(kContextOffset);
}

inline void RawToken::setContext(RawObject context) const {
  instanceVariableAtPut(kContextOffset, context);
}

inline RawObject RawToken::oldValue() const {
  return instanceVariableAt(kOldValueOffset);
}

inline void RawToken::setOldValue(RawObject old_value) const {
  instanceVariableAtPut(kOldValueOffset, old_value);
}

inline bool RawToken::used() const {
  return RawBool::cast(instanceVariableAt(kUsedOffset)).value();
}

inline void RawToken::setUsed(bool used) const {
  instanceVariableAtPut(kUsedOffset, RawBool::fromBool(used));
}

inline RawObject RawToken::var() const {
  return instanceVariableAt(kVarOffset);
}

inline void RawToken::setVar(RawObject var) const {
  instanceVariableAtPut(kVarOffset, var);
}

// RawWeakRef

inline RawObject RawWeakRef::referent() const {
  return instanceVariableAt(kReferentOffset);
}

inline void RawWeakRef::setReferent(RawObject referent) const {
  instanceVariableAtPut(kReferentOffset, referent);
}

inline RawObject RawWeakRef::callback() const {
  return instanceVariableAt(kCallbackOffset);
}

inline void RawWeakRef::setCallback(RawObject callable) const {
  instanceVariableAtPut(kCallbackOffset, callable);
}

inline RawObject RawWeakRef::link() const {
  return instanceVariableAt(kLinkOffset);
}

inline void RawWeakRef::setLink(RawObject reference) const {
  instanceVariableAtPut(kLinkOffset, reference);
}

inline RawObject RawWeakRef::hash() const {
  return instanceVariableAt(kHashOffset);
}

inline void RawWeakRef::setHash(RawObject hash) const {
  instanceVariableAtPut(kHashOffset, hash);
}

// RawUserWeakRefBase

inline RawObject RawUserWeakRefBase::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawUserWeakRefBase::setValue(RawObject value) const {
  DCHECK(value.isWeakRef(), "Only tuple type is permitted as a value");
  instanceVariableAtPut(kValueOffset, value);
}

inline RawWeakRef weakRefUnderlying(RawObject object) {
  if (object.isWeakRef()) {
    return RawWeakRef::cast(object);
  }
  return RawWeakRef::cast(object.rawCast<RawUserWeakRefBase>().value());
}

// RawWeakLink

inline RawObject RawWeakLink::next() const {
  return instanceVariableAt(kNextOffset);
}

inline void RawWeakLink::setNext(RawObject object) const {
  instanceVariableAtPut(kNextOffset, object);
}

inline RawObject RawWeakLink::prev() const {
  return instanceVariableAt(kPrevOffset);
}

inline void RawWeakLink::setPrev(RawObject object) const {
  instanceVariableAtPut(kPrevOffset, object);
}

// RawLayout

inline LayoutId RawLayout::id() const {
  return static_cast<LayoutId>(header().hashCode());
}

inline void RawLayout::setId(LayoutId id) const {
  setHeader(header().withHashCode(static_cast<word>(id)));
}

inline void RawLayout::setDescribedType(RawObject type) const {
  instanceVariableAtPut(kDescribedTypeOffset, type);
}

inline RawObject RawLayout::describedType() const {
  return instanceVariableAt(kDescribedTypeOffset);
}

inline void RawLayout::setInObjectAttributes(RawObject attributes) const {
  instanceVariableAtPut(kInObjectAttributesOffset, attributes);
}

inline RawObject RawLayout::inObjectAttributes() const {
  return instanceVariableAt(kInObjectAttributesOffset);
}

inline void RawLayout::setOverflowAttributes(RawObject attributes) const {
  instanceVariableAtPut(kOverflowAttributesOffset, attributes);
}

inline void RawLayout::setDictOverflowOffset(word offset) const {
  instanceVariableAtPut(kOverflowAttributesOffset,
                        RawSmallInt::fromWord(offset));
}

inline word RawLayout::dictOverflowOffset() const {
  return RawSmallInt::cast(instanceVariableAt(kOverflowAttributesOffset))
      .value();
}

inline word RawLayout::instanceSize() const {
  word instance_size_in_words = numInObjectAttributes();
  if (!isSealed()) {
    instance_size_in_words += 1;
  }
  if (isNativeProxyLayout()) {
    instance_size_in_words += RawNativeProxy::kSizeFromEnd / kPointerSize;
  }
  return instance_size_in_words * kPointerSize;
}

inline bool RawLayout::hasDictOverflow() const {
  return overflowAttributes().isSmallInt();
}

inline bool RawLayout::hasTupleOverflow() const {
  return overflowAttributes().isTuple();
}

inline RawObject RawLayout::overflowAttributes() const {
  return instanceVariableAt(kOverflowAttributesOffset);
}

inline void RawLayout::setAdditions(RawObject additions) const {
  instanceVariableAtPut(kAdditionsOffset, additions);
}

inline RawObject RawLayout::additions() const {
  return instanceVariableAt(kAdditionsOffset);
}

inline void RawLayout::setDeletions(RawObject deletions) const {
  instanceVariableAtPut(kDeletionsOffset, deletions);
}

inline RawObject RawLayout::deletions() const {
  return instanceVariableAt(kDeletionsOffset);
}

inline word RawLayout::overflowOffset() const {
  DCHECK(hasTupleOverflow() || hasDictOverflow(),
         "must have tuple or dict overflow");
  return numInObjectAttributes() * kPointerSize;
}

inline word RawLayout::numInObjectAttributes() const {
  return RawSmallInt::cast(instanceVariableAt(kNumInObjectAttributesOffset))
      .value();
}

inline void RawLayout::setNumInObjectAttributes(word count) const {
  instanceVariableAtPut(kNumInObjectAttributesOffset,
                        RawSmallInt::fromWord(count));
}

inline void RawLayout::seal() const {
  setOverflowAttributes(RawNoneType::object());
}

inline bool RawLayout::isSealed() const {
  return overflowAttributes().isNoneType();
}

inline bool RawLayout::isNativeProxyLayout() const {
  RawObject described_type = describedType();
  if (described_type.isNoneType()) {
    return false;
  }
  return described_type.rawCast<RawType>().hasNativeData();
}

// RawSetIterator

inline word RawSetIterator::consumedCount() const {
  return RawSmallInt::cast(instanceVariableAt(kConsumedCountOffset)).value();
}

inline void RawSetIterator::setConsumedCount(word consumed) const {
  instanceVariableAtPut(kConsumedCountOffset, RawSmallInt::fromWord(consumed));
}

// RawIteratorBase

inline RawObject RawIteratorBase::iterable() const {
  return instanceVariableAt(kIterableOffset);
}

inline void RawIteratorBase::setIterable(RawObject iterable) const {
  instanceVariableAtPut(kIterableOffset, iterable);
}

inline word RawIteratorBase::index() const {
  return RawSmallInt::cast(instanceVariableAt(kIndexOffset)).value();
}

inline void RawIteratorBase::setIndex(word index) const {
  instanceVariableAtPut(kIndexOffset, RawSmallInt::fromWord(index));
}

// RawLongRangeIterator

inline RawObject RawLongRangeIterator::next() const {
  return instanceVariableAt(kNextOffset);
}

inline void RawLongRangeIterator::setNext(RawObject next) const {
  return instanceVariableAtPut(kNextOffset, next);
}

inline RawObject RawLongRangeIterator::stop() const {
  return instanceVariableAt(kStopOffset);
}

inline void RawLongRangeIterator::setStop(RawObject stop) const {
  return instanceVariableAtPut(kStopOffset, stop);
}

inline RawObject RawLongRangeIterator::step() const {
  return instanceVariableAt(kStepOffset);
}

inline void RawLongRangeIterator::setStep(RawObject step) const {
  return instanceVariableAtPut(kStepOffset, step);
}

// RawRangeIterator

inline word RawRangeIterator::next() const {
  return RawSmallInt::cast(instanceVariableAt(kNextOffset)).value();
}

inline void RawRangeIterator::setNext(word next) const {
  return instanceVariableAtPut(kNextOffset, RawSmallInt::fromWord(next));
}

inline word RawRangeIterator::step() const {
  return RawSmallInt::cast(instanceVariableAt(kStepOffset)).value();
}

inline void RawRangeIterator::setStep(word step) const {
  return instanceVariableAtPut(kStepOffset, RawSmallInt::fromWord(step));
}

inline word RawRangeIterator::length() const {
  return RawSmallInt::cast(instanceVariableAt(kLengthOffset)).value();
}

inline void RawRangeIterator::setLength(word length) const {
  return instanceVariableAtPut(kLengthOffset, RawSmallInt::fromWord(length));
}

// RawSuper

inline RawObject RawSuper::type() const {
  return instanceVariableAt(kTypeOffset);
}

inline void RawSuper::setType(RawObject type) const {
  instanceVariableAtPut(kTypeOffset, type);
}

inline RawObject RawSuper::object() const {
  return instanceVariableAt(kObjectOffset);
}

inline void RawSuper::setObject(RawObject obj) const {
  instanceVariableAtPut(kObjectOffset, obj);
}

inline RawObject RawSuper::objectType() const {
  return instanceVariableAt(kObjectTypeOffset);
}

inline void RawSuper::setObjectType(RawObject type) const {
  instanceVariableAtPut(kObjectTypeOffset, type);
}

// RawTupleIterator

inline word RawTupleIterator::length() const {
  return RawSmallInt::cast(instanceVariableAt(kLengthOffset)).value();
}

inline void RawTupleIterator::setLength(word length) const {
  instanceVariableAtPut(kLengthOffset, RawSmallInt::fromWord(length));
}

// RawExceptionState

inline RawObject RawExceptionState::type() const {
  return instanceVariableAt(kTypeOffset);
}

inline RawObject RawExceptionState::value() const {
  return instanceVariableAt(kValueOffset);
}

inline RawObject RawExceptionState::traceback() const {
  return instanceVariableAt(kTracebackOffset);
}

inline void RawExceptionState::setType(RawObject type) const {
  instanceVariableAtPut(kTypeOffset, type);
}

inline void RawExceptionState::setValue(RawObject value) const {
  instanceVariableAtPut(kValueOffset, value);
}

inline void RawExceptionState::setTraceback(RawObject tb) const {
  instanceVariableAtPut(kTracebackOffset, tb);
}

inline RawObject RawExceptionState::previous() const {
  return instanceVariableAt(kPreviousOffset);
}

inline void RawExceptionState::setPrevious(RawObject prev) const {
  instanceVariableAtPut(kPreviousOffset, prev);
}

// RawGeneratorBase

inline RawObject RawGeneratorBase::generatorFrame() const {
  return instanceVariableAt(kFrameOffset);
}

inline void RawGeneratorBase::setGeneratorFrame(RawObject obj) const {
  instanceVariableAtPut(kFrameOffset, obj);
}

inline RawObject RawGeneratorBase::exceptionState() const {
  return instanceVariableAt(kExceptionStateOffset);
}

inline void RawGeneratorBase::setExceptionState(RawObject obj) const {
  instanceVariableAtPut(kExceptionStateOffset, obj);
}

inline RawObject RawGeneratorBase::name() const {
  return instanceVariableAt(kNameOffset);
}

inline void RawGeneratorBase::setName(RawObject obj) const {
  instanceVariableAtPut(kNameOffset, obj);
}

inline RawObject RawGeneratorBase::running() const {
  return instanceVariableAt(kRunningOffset);
}

inline void RawGeneratorBase::setRunning(RawObject obj) const {
  instanceVariableAtPut(kRunningOffset, obj);
}

inline RawObject RawGeneratorBase::qualname() const {
  return instanceVariableAt(kQualnameOffset);
}

inline void RawGeneratorBase::setQualname(RawObject obj) const {
  instanceVariableAtPut(kQualnameOffset, obj);
}

// RawGeneratorFrame

inline Frame* RawGeneratorFrame::frame() const {
  return reinterpret_cast<Frame*>(address() + kFrameOffset +
                                  maxStackSize() * kPointerSize);
}

inline RawObject RawGeneratorFrame::function() const {
  return instanceVariableAt(kFrameOffset +
                            (numFrameWords() - 1) * kPointerSize);
}

inline word RawGeneratorFrame::numAttributes(word extra_words) {
  return kNumOverheadWords + kFrameSize / kPointerSize + extra_words;
}

inline word RawGeneratorFrame::numFrameWords() const {
  return headerCountOrOverflow() - kNumOverheadWords;
}

inline word RawGeneratorFrame::maxStackSize() const {
  return RawSmallInt::cast(instanceVariableAt(kMaxStackSizeOffset)).value();
}

inline void RawGeneratorFrame::setMaxStackSize(word offset) const {
  instanceVariableAtPut(kMaxStackSizeOffset, RawSmallInt::fromWord(offset));
}

inline void RawGeneratorFrame::setStackSize(word size) const {
  word offset =
      maxStackSize() * kPointerSize + kFrameOffset + kStackSizeFrameOffset;
  instanceVariableAtPut(offset, RawSmallInt::fromWord(size));
}

inline word RawGeneratorFrame::stackSize() const {
  word offset =
      maxStackSize() * kPointerSize + kFrameOffset + kStackSizeFrameOffset;
  return RawSmallInt::cast(instanceVariableAt(offset)).value();
}

// RawCoroutineWrapper

inline RawObject RawCoroutineWrapper::coroutine() const {
  return instanceVariableAt(kCoroutineOffset);
}

inline void RawCoroutineWrapper::setCoroutine(RawObject coroutine) const {
  instanceVariableAtPut(kCoroutineOffset, coroutine);
}

// RawUnderIOBase

inline bool RawUnderIOBase::closed() const {
  RawObject closed = instanceVariableAt(kClosedOffset);
  return closed.isBool() && RawBool::cast(closed).value();
}

inline void RawUnderIOBase::setClosed(bool closed) const {
  instanceVariableAtPut(kClosedOffset, RawBool::fromBool(closed));
}

// RawUnderBufferedIOMixin

inline RawObject RawUnderBufferedIOMixin::underlying() const {
  return instanceVariableAt(kUnderlyingOffset);
}

inline void RawUnderBufferedIOMixin::setUnderlying(RawObject value) const {
  instanceVariableAtPut(kUnderlyingOffset, value);
}

// RawBufferedRandom

inline word RawBufferedRandom::bufferSize() const {
  return RawSmallInt::cast(instanceVariableAt(kBufferSizeOffset)).value();
}

inline void RawBufferedRandom::setBufferSize(word buffer_size) const {
  instanceVariableAtPut(kBufferSizeOffset, RawSmallInt::fromWord(buffer_size));
}

inline RawObject RawBufferedRandom::reader() const {
  return instanceVariableAt(kReaderOffset);
}

inline void RawBufferedRandom::setReader(RawObject reader) const {
  instanceVariableAtPut(kReaderOffset, reader);
}

inline RawObject RawBufferedRandom::writeBuf() const {
  return instanceVariableAt(kWriteBufOffset);
}

inline void RawBufferedRandom::setWriteBuf(RawObject write_buf) const {
  instanceVariableAtPut(kWriteBufOffset, write_buf);
}

inline RawObject RawBufferedRandom::writeLock() const {
  return instanceVariableAt(kWriteLockOffset);
}

inline void RawBufferedRandom::setWriteLock(RawObject write_lock) const {
  instanceVariableAtPut(kWriteLockOffset, write_lock);
}

// RawBufferedReader

inline word RawBufferedReader::bufferSize() const {
  return RawSmallInt::cast(instanceVariableAt(kBufferSizeOffset)).value();
}

inline void RawBufferedReader::setBufferSize(word buffer_size) const {
  instanceVariableAtPut(kBufferSizeOffset, RawSmallInt::fromWord(buffer_size));
}

inline RawObject RawBufferedReader::readBuf() const {
  return instanceVariableAt(kReadBufOffset);
}

inline void RawBufferedReader::setReadBuf(RawObject read_buf) const {
  instanceVariableAtPut(kReadBufOffset, read_buf);
}

inline word RawBufferedReader::readPos() const {
  return RawSmallInt::cast(instanceVariableAt(kReadPosOffset)).value();
}

inline void RawBufferedReader::setReadPos(word read_pos) const {
  instanceVariableAtPut(kReadPosOffset, RawSmallInt::fromWord(read_pos));
}

inline word RawBufferedReader::bufferNumBytes() const {
  return RawSmallInt::cast(instanceVariableAt(kBufferNumBytesOffset)).value();
}

inline void RawBufferedReader::setBufferNumBytes(word buffer_num_bytes) const {
  instanceVariableAtPut(kBufferNumBytesOffset,
                        RawSmallInt::fromWord(buffer_num_bytes));
}

// RawBufferedWriter

inline word RawBufferedWriter::bufferSize() const {
  return RawSmallInt::cast(instanceVariableAt(kBufferSizeOffset)).value();
}

inline void RawBufferedWriter::setBufferSize(RawObject buffer_size) const {
  instanceVariableAtPut(kBufferSizeOffset, buffer_size);
}

inline RawObject RawBufferedWriter::writeBuf() const {
  return instanceVariableAt(kWriteBufOffset);
}

inline void RawBufferedWriter::setWriteBuf(RawObject write_buf) const {
  instanceVariableAtPut(kWriteBufOffset, write_buf);
}

inline RawObject RawBufferedWriter::writeLock() const {
  return instanceVariableAt(kWriteLockOffset);
}

inline void RawBufferedWriter::setWriteLock(RawObject write_lock) const {
  instanceVariableAtPut(kWriteLockOffset, write_lock);
}

// RawBytesIO

inline RawObject RawBytesIO::buffer() const {
  return instanceVariableAt(kBufferOffset);
}

inline void RawBytesIO::setBuffer(RawObject buffer) const {
  instanceVariableAtPut(kBufferOffset, buffer);
}

inline word RawBytesIO::numItems() const {
  return RawSmallInt::cast(instanceVariableAt(kNumItemsOffset)).value();
}

inline void RawBytesIO::setNumItems(word num_items) const {
  instanceVariableAtPut(kNumItemsOffset, RawSmallInt::fromWord(num_items));
}

inline word RawBytesIO::pos() const {
  return RawSmallInt::cast(instanceVariableAt(kPosOffset)).value();
}

inline void RawBytesIO::setPos(word pos) const {
  instanceVariableAtPut(kPosOffset, RawSmallInt::fromWord(pos));
}

// RawFileIO

inline RawObject RawFileIO::fd() const { return instanceVariableAt(kFdOffset); }

inline void RawFileIO::setFd(RawObject fd) const {
  instanceVariableAtPut(kFdOffset, fd);
}

inline RawObject RawFileIO::isCreated() const {
  return instanceVariableAt(kCreatedOffset);
}

inline void RawFileIO::setCreated(RawObject value) const {
  instanceVariableAtPut(kCreatedOffset, value);
}

inline RawObject RawFileIO::isReadable() const {
  return instanceVariableAt(kReadableOffset);
}

inline void RawFileIO::setReadable(RawObject value) const {
  instanceVariableAtPut(kReadableOffset, value);
}

inline RawObject RawFileIO::isWritable() const {
  return instanceVariableAt(kWritableOffset);
}

inline void RawFileIO::setWritable(RawObject value) const {
  instanceVariableAtPut(kWritableOffset, value);
}

inline RawObject RawFileIO::isAppending() const {
  return instanceVariableAt(kAppendingOffset);
}

inline void RawFileIO::setAppending(RawObject value) const {
  instanceVariableAtPut(kAppendingOffset, value);
}

inline RawObject RawFileIO::seekable() const {
  return instanceVariableAt(kSeekableOffset);
}

inline void RawFileIO::setSeekable(RawObject value) const {
  instanceVariableAtPut(kSeekableOffset, value);
}

inline RawObject RawFileIO::shouldCloseFd() const {
  return instanceVariableAt(kCloseFdOffset);
}

inline void RawFileIO::setShouldCloseFd(RawObject value) const {
  instanceVariableAtPut(kCloseFdOffset, value);
}

// RawStringIO

inline RawObject RawStringIO::buffer() const {
  return instanceVariableAt(kBufferOffset);
}

inline void RawStringIO::setBuffer(RawObject buffer) const {
  instanceVariableAtPut(kBufferOffset, buffer);
}

inline word RawStringIO::pos() const {
  return RawSmallInt::cast(instanceVariableAt(kPosOffset)).value();
}

inline void RawStringIO::setPos(word new_pos) const {
  instanceVariableAtPut(kPosOffset, RawSmallInt::fromWord(new_pos));
}

inline RawObject RawStringIO::readnl() const {
  return instanceVariableAt(kReadnlOffset);
}

inline void RawStringIO::setReadnl(RawObject readnl) const {
  instanceVariableAtPut(kReadnlOffset, readnl);
}

inline bool RawStringIO::hasReadtranslate() const {
  return RawBool::cast(instanceVariableAt(kReadtranslateOffset)).value();
}

inline void RawStringIO::setReadtranslate(bool readtranslate) const {
  instanceVariableAtPut(kReadtranslateOffset, RawBool::fromBool(readtranslate));
}

inline bool RawStringIO::hasReaduniversal() const {
  return RawBool::cast(instanceVariableAt(kReaduniversalOffset)).value();
}

inline void RawStringIO::setReaduniversal(bool readuniversal) const {
  instanceVariableAtPut(kReaduniversalOffset, RawBool::fromBool(readuniversal));
}

inline RawObject RawStringIO::seennl() const {
  return instanceVariableAt(kSeennlOffset);
}

inline void RawStringIO::setSeennl(RawObject seennl) const {
  instanceVariableAtPut(kSeennlOffset, seennl);
}

inline RawObject RawStringIO::writenl() const {
  return instanceVariableAt(kWritenlOffset);
}

inline void RawStringIO::setWritenl(RawObject writenl) const {
  instanceVariableAtPut(kWritenlOffset, writenl);
}

inline bool RawStringIO::hasWritetranslate() const {
  return RawBool::cast(instanceVariableAt(kWritetranslateOffset)).value();
}

inline void RawStringIO::setWritetranslate(bool writetranslate) const {
  instanceVariableAtPut(kWritetranslateOffset,
                        RawBool::fromBool(writetranslate));
}

// RawInstanceMethod

inline RawObject RawInstanceMethod::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawInstanceMethod::setFunction(RawObject function) const {
  return instanceVariableAtPut(kFunctionOffset, function);
}

// RawIncrementalNewlineDecoder

inline RawObject RawIncrementalNewlineDecoder::errors() const {
  return instanceVariableAt(kErrorsOffset);
}

inline void RawIncrementalNewlineDecoder::setErrors(RawObject errors) const {
  instanceVariableAtPut(kErrorsOffset, errors);
}

inline RawObject RawIncrementalNewlineDecoder::translate() const {
  return instanceVariableAt(kTranslateOffset);
}

inline void RawIncrementalNewlineDecoder::setTranslate(
    RawObject translate) const {
  instanceVariableAtPut(kTranslateOffset, translate);
}

inline RawObject RawIncrementalNewlineDecoder::decoder() const {
  return instanceVariableAt(kDecoderOffset);
}

inline void RawIncrementalNewlineDecoder::setDecoder(RawObject decoder) const {
  instanceVariableAtPut(kDecoderOffset, decoder);
}

inline RawObject RawIncrementalNewlineDecoder::seennl() const {
  return instanceVariableAt(kSeennlOffset);
}

inline void RawIncrementalNewlineDecoder::setSeennl(RawObject seennl) const {
  instanceVariableAtPut(kSeennlOffset, seennl);
}

inline RawObject RawIncrementalNewlineDecoder::pendingcr() const {
  return instanceVariableAt(kPendingcrOffset);
}

inline void RawIncrementalNewlineDecoder::setPendingcr(
    RawObject pendingcr) const {
  instanceVariableAtPut(kPendingcrOffset, pendingcr);
}

// RawTextIOWrapper

inline RawObject RawTextIOWrapper::buffer() const {
  return instanceVariableAt(kBufferOffset);
}

inline void RawTextIOWrapper::setBuffer(RawObject buffer) const {
  instanceVariableAtPut(kBufferOffset, buffer);
}

inline bool RawTextIOWrapper::detached() const { return buffer().isNoneType(); }

inline bool RawTextIOWrapper::lineBuffering() const {
  return RawBool::cast(instanceVariableAt(kLineBufferingOffset)).value();
}

inline void RawTextIOWrapper::setLineBuffering(RawObject line_buffering) const {
  instanceVariableAtPut(kLineBufferingOffset, line_buffering);
}

inline RawObject RawTextIOWrapper::encoding() const {
  return instanceVariableAt(kEncodingOffset);
}

inline void RawTextIOWrapper::setEncoding(RawObject encoding) const {
  instanceVariableAtPut(kEncodingOffset, encoding);
}

inline RawObject RawTextIOWrapper::errors() const {
  return instanceVariableAt(kErrorsOffset);
}

inline void RawTextIOWrapper::setErrors(RawObject errors) const {
  instanceVariableAtPut(kErrorsOffset, errors);
}

inline bool RawTextIOWrapper::readuniversal() const {
  return RawBool::cast(instanceVariableAt(kReaduniversalOffset)).value();
}

inline void RawTextIOWrapper::setReaduniversal(RawObject readuniversal) const {
  instanceVariableAtPut(kReaduniversalOffset, readuniversal);
}

inline bool RawTextIOWrapper::readtranslate() const {
  return RawBool::cast(instanceVariableAt(kReadtranslateOffset)).value();
}

inline void RawTextIOWrapper::setReadtranslate(RawObject readtranslate) const {
  instanceVariableAtPut(kReadtranslateOffset, readtranslate);
}

inline RawObject RawTextIOWrapper::readnl() const {
  return instanceVariableAt(kReadnlOffset);
}

inline void RawTextIOWrapper::setReadnl(RawObject readnl) const {
  instanceVariableAtPut(kReadnlOffset, readnl);
}

inline bool RawTextIOWrapper::writetranslate() const {
  return RawBool::cast(instanceVariableAt(kWritetranslateOffset)).value();
}

inline void RawTextIOWrapper::setWritetranslate(
    RawObject writetranslate) const {
  instanceVariableAtPut(kWritetranslateOffset, writetranslate);
}

inline RawObject RawTextIOWrapper::writenl() const {
  return instanceVariableAt(kWritenlOffset);
}

inline void RawTextIOWrapper::setWritenl(RawObject writenl) const {
  instanceVariableAtPut(kWritenlOffset, writenl);
}

inline RawObject RawTextIOWrapper::encoder() const {
  return instanceVariableAt(kEncoderOffset);
}

inline void RawTextIOWrapper::setEncoder(RawObject encoder) const {
  instanceVariableAtPut(kEncoderOffset, encoder);
}

inline RawObject RawTextIOWrapper::decoder() const {
  return instanceVariableAt(kDecoderOffset);
}

inline void RawTextIOWrapper::setDecoder(RawObject decoder) const {
  instanceVariableAtPut(kDecoderOffset, decoder);
}

inline RawObject RawTextIOWrapper::decodedChars() const {
  return instanceVariableAt(kDecodedCharsOffset);
}

inline void RawTextIOWrapper::setDecodedChars(RawObject decoded_chars) const {
  instanceVariableAtPut(kDecodedCharsOffset, decoded_chars);
}

inline RawObject RawTextIOWrapper::decodedCharsUsed() const {
  return instanceVariableAt(kDecodedCharsUsedOffset);
}

inline void RawTextIOWrapper::setDecodedCharsUsed(
    RawObject decoded_chars_used) const {
  instanceVariableAtPut(kDecodedCharsUsedOffset, decoded_chars_used);
}

inline RawObject RawTextIOWrapper::snapshot() const {
  return instanceVariableAt(kSnapshotOffset);
}

inline void RawTextIOWrapper::setSnapshot(RawObject snapshot) const {
  instanceVariableAtPut(kSnapshotOffset, snapshot);
}

inline RawObject RawTextIOWrapper::seekable() const {
  return instanceVariableAt(kSeekableOffset);
}

inline void RawTextIOWrapper::setSeekable(RawObject seekable) const {
  instanceVariableAtPut(kSeekableOffset, seekable);
}

inline RawObject RawTextIOWrapper::hasRead1() const {
  return instanceVariableAt(kHasRead1Offset);
}

inline void RawTextIOWrapper::setHasRead1(RawObject has_read1) const {
  instanceVariableAtPut(kHasRead1Offset, has_read1);
}

inline RawObject RawTextIOWrapper::b2cratio() const {
  return instanceVariableAt(kB2cratioOffset);
}

inline void RawTextIOWrapper::setB2cratio(RawObject b2cratio) const {
  instanceVariableAtPut(kB2cratioOffset, b2cratio);
}

inline RawObject RawTextIOWrapper::telling() const {
  return instanceVariableAt(kTellingOffset);
}

inline void RawTextIOWrapper::setTelling(RawObject telling) const {
  instanceVariableAtPut(kTellingOffset, telling);
}

// RawAsyncGenerator

inline RawObject RawAsyncGenerator::finalizer() const {
  return instanceVariableAt(kFinalizerOffset);
}

inline void RawAsyncGenerator::setFinalizer(RawObject finalizer) const {
  instanceVariableAtPut(kFinalizerOffset, finalizer);
}

inline bool RawAsyncGenerator::hooksInited() const {
  return RawBool::cast(instanceVariableAt(kHooksInitedOffset)).value();
}

inline void RawAsyncGenerator::setHooksInited(bool hooks_inited) const {
  instanceVariableAtPut(kHooksInitedOffset, RawBool::fromBool(hooks_inited));
}

// RawAsyncGeneratorOpIterBase

inline RawObject RawAsyncGeneratorOpIterBase::generator() const {
  return instanceVariableAt(kGeneratorOffset);
}

inline void RawAsyncGeneratorOpIterBase::setGenerator(
    RawObject generator) const {
  instanceVariableAtPut(kGeneratorOffset, generator);
}

inline RawAsyncGeneratorOpIterBase::State RawAsyncGeneratorOpIterBase::state()
    const {
  return static_cast<State>(
      RawSmallInt::cast(instanceVariableAt(kStateOffset)).value());
}

inline void RawAsyncGeneratorOpIterBase::setState(State state) const {
  instanceVariableAtPut(kStateOffset,
                        RawSmallInt::fromWord(static_cast<word>(state)));
}

// RawAsyncGeneratorAsend

inline RawObject RawAsyncGeneratorAsend::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawAsyncGeneratorAsend::setValue(RawObject value) const {
  instanceVariableAtPut(kValueOffset, value);
}

// RawAsyncGeneratorAthrow

inline RawObject RawAsyncGeneratorAthrow::exceptionTraceback() const {
  return instanceVariableAt(kExceptionTracebackOffset);
}

inline void RawAsyncGeneratorAthrow::setExceptionTraceback(
    RawObject exception_traceback) const {
  instanceVariableAtPut(kExceptionTracebackOffset, exception_traceback);
}

inline RawObject RawAsyncGeneratorAthrow::exceptionType() const {
  return instanceVariableAt(kExceptionTypeOffset);
}

inline void RawAsyncGeneratorAthrow::setExceptionType(
    RawObject exception_type) const {
  instanceVariableAtPut(kExceptionTypeOffset, exception_type);
}

inline RawObject RawAsyncGeneratorAthrow::exceptionValue() const {
  return instanceVariableAt(kExceptionValueOffset);
}

inline void RawAsyncGeneratorAthrow::setExceptionValue(
    RawObject exception_value) const {
  instanceVariableAtPut(kExceptionValueOffset, exception_value);
}

// RawAsyncGeneratorWrappedValue

inline RawObject RawAsyncGeneratorWrappedValue::value() const {
  return instanceVariableAt(kValueOffset);
}

inline void RawAsyncGeneratorWrappedValue::setValue(RawObject value) const {
  instanceVariableAtPut(kValueOffset, value);
}

// RawTraceback

inline RawObject RawTraceback::function() const {
  return instanceVariableAt(kFunctionOffset);
}

inline void RawTraceback::setFunction(RawObject function) const {
  instanceVariableAtPut(kFunctionOffset, function);
}

inline RawObject RawTraceback::lasti() const {
  return instanceVariableAt(kLastiOffset);
}

inline void RawTraceback::setLasti(RawObject lasti) const {
  instanceVariableAtPut(kLastiOffset, lasti);
}

inline RawObject RawTraceback::lineno() const {
  return instanceVariableAt(kLinenoOffset);
}

inline void RawTraceback::setLineno(RawObject lineno) const {
  instanceVariableAtPut(kLinenoOffset, lineno);
}

inline RawObject RawTraceback::next() const {
  return instanceVariableAt(kNextOffset);
}

inline void RawTraceback::setNext(RawObject next) const {
  instanceVariableAtPut(kNextOffset, next);
}

}  // namespace py
