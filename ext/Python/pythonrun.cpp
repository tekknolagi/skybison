// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "cpython-data.h"
#include "cpython-func.h"

#include "api-handle.h"
#include "builtins-module.h"
#include "compile-utils.h"
#include "exception-builtins.h"
#include "fileutils.h"
#include "marshal.h"
#include "module-builtins.h"
#include "modules.h"
#include "object-builtins.h"
#include "os.h"
#include "runtime.h"
#include "sys-module.h"

namespace py {

PY_EXPORT PyObject* Py_CompileString(const char* str, const char* filename,
                                     int start) {
  DCHECK(str != nullptr, "str must not be null");
  DCHECK(filename != nullptr, "filename must not be null");
  PyObject* filename_obj = PyUnicode_DecodeFSDefault(filename);
  if (filename_obj == nullptr) return nullptr;

  PyArena* arena = PyArena_New();
  if (arena == nullptr) {
    Py_DECREF(filename_obj);
    return nullptr;
  }

  struct _mod* mod =
      PyParser_ASTFromStringObject(str, filename_obj, start, nullptr, arena);
  if (mod == nullptr) {
    PyArena_Free(arena);
    Py_DECREF(filename_obj);
    return nullptr;
  }
  PyObject* result = reinterpret_cast<PyObject*>(
      PyAST_CompileObject(mod, filename_obj, nullptr, -1, arena));
  PyArena_Free(arena);
  Py_DECREF(filename_obj);
  return result;
}

PY_EXPORT int PyRun_AnyFile(FILE* fp, const char* filename) {
  return PyRun_AnyFileExFlags(fp, filename, /*closeit=*/0, /*flags=*/nullptr);
}

PY_EXPORT int PyRun_AnyFileEx(FILE* fp, const char* filename, int closeit) {
  return PyRun_AnyFileExFlags(fp, filename, closeit, /*flags=*/nullptr);
}

PY_EXPORT int PyRun_AnyFileExFlags(FILE* fp, const char* filename, int closeit,
                                   PyCompilerFlags* flags) {
  if (filename == nullptr) {
    filename = "???";
  }
  if (Py_FdIsInteractive(fp, filename)) {
    int err = PyRun_InteractiveLoopFlags(fp, filename, flags);
    if (closeit) fclose(fp);
    return err;
  }
  return PyRun_SimpleFileExFlags(fp, filename, closeit, flags);
}

PY_EXPORT int PyRun_AnyFileFlags(FILE* fp, const char* filename,
                                 PyCompilerFlags* flags) {
  return PyRun_AnyFileExFlags(fp, filename, /*closeit=*/0, flags);
}

static PyObject* runMod(struct _mod* mod, PyObject* filename, PyObject* globals,
                        PyObject* locals, PyCompilerFlags* flags,
                        PyArena* arena) {
  PyCodeObject* code = PyAST_CompileObject(mod, filename, flags, -1, arena);
  if (code == nullptr) return nullptr;
  PyObject* v =
      PyEval_EvalCode(reinterpret_cast<PyObject*>(code), globals, locals);
  Py_DECREF(code);
  return v;
}

static PyObject* runPycFile(FILE* fp, const char* filename, Module& module,
                            PyCompilerFlags* flags) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  word file_len;
  unique_c_ptr<byte> buffer(OS::readFile(fp, &file_len));
  if (buffer == nullptr) {
    std::fprintf(stderr, "Could not read file '%s'\n", filename);
    std::fclose(fp);
    return nullptr;
  }

  Object code_obj(&scope, NoneType::object());
  Marshal::Reader reader(&scope, thread, View<byte>(buffer.get(), file_len));
  Str filename_str(&scope, runtime->newStrFromCStr(filename));
  if (reader.readPycHeader(filename_str).isErrorException()) {
    return nullptr;
  }
  code_obj = reader.readObject();
  std::fclose(fp);

  Code code(&scope, *code_obj);
  RawObject result = executeModule(thread, code, module);
  if (!result.isError() && flags) {
    flags->cf_flags |= (code.flags() & PyCF_MASK);
  }
  return result.isError() ? nullptr : ApiHandle::newReference(runtime, result);
}

static PyObject* moduleProxy(PyObject* module_obj) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);
  Module module(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(module_obj)));
  return ApiHandle::borrowedReference(thread->runtime(), module.moduleProxy());
}

// A PyRun_InteractiveOneObject() auxiliary function that does not print the
// error on failure.
static int PyRun_InteractiveOneObjectEx(FILE* fp, PyObject* filename,
                                        PyCompilerFlags* flags) {
  PyObject* mod_name = PyUnicode_InternFromString("__main__");
  if (mod_name == nullptr) {
    return -1;
  }
  // TODO(T46532201): If fp == stdin, fetch encoding from sys.stdin if possible
  const char* ps1 = "";
  const char* ps2 = "";
  PyObject* ps1_obj = PySys_GetObject("ps1");
  if (ps1_obj != nullptr) {
    if ((ps1_obj = PyObject_Str(ps1_obj)) == nullptr) {
      PyErr_Clear();
    } else if (PyUnicode_Check(ps1_obj)) {
      if ((ps1 = PyUnicode_AsUTF8(ps1_obj)) == nullptr) {
        PyErr_Clear();
        ps1 = "";
      }
    }
    Py_XDECREF(ps1_obj);
  }
  PyObject* ps2_obj = PySys_GetObject("ps2");
  if (ps2_obj != nullptr) {
    if ((ps2_obj = PyObject_Str(ps2_obj)) == nullptr) {
      PyErr_Clear();
    } else if (PyUnicode_Check(ps2_obj)) {
      if ((ps2 = PyUnicode_AsUTF8(ps2_obj)) == nullptr) {
        PyErr_Clear();
        ps2 = "";
      }
    }
    Py_XDECREF(ps2_obj);
  }
  PyArena* arena = PyArena_New();
  if (arena == nullptr) {
    Py_DECREF(mod_name);
    return -1;
  }
  char* enc = nullptr;
  int errcode = 0;
  struct _mod* mod = PyParser_ASTFromFileObject(
      fp, filename, enc, Py_single_input, ps1, ps2, flags, &errcode, arena);
  if (mod == nullptr) {
    Py_DECREF(mod_name);
    PyArena_Free(arena);
    if (errcode == E_EOF) {
      PyErr_Clear();
      return E_EOF;
    }
    return -1;
  }
  PyObject* module = PyImport_AddModuleObject(mod_name);
  Py_DECREF(mod_name);
  if (module == nullptr) {
    PyArena_Free(arena);
    return -1;
  }
  PyObject* module_proxy = moduleProxy(module);
  PyObject* result = runMod(mod, filename, /*globals=*/module_proxy,
                            /*locals=*/module_proxy, flags, arena);
  PyArena_Free(arena);
  if (result == nullptr) {
    return -1;
  }
  Py_DECREF(result);
  flushStdFiles();
  return 0;
}

PY_EXPORT int PyRun_InteractiveLoop(FILE* fp, const char* filename) {
  return PyRun_InteractiveLoopFlags(fp, filename, /*flags=*/nullptr);
}

PY_EXPORT int PyRun_InteractiveLoopFlags(FILE* fp, const char* filename,
                                         PyCompilerFlags* flags) {
  PyObject* filename_str = PyUnicode_DecodeFSDefault(filename);
  if (filename_str == nullptr) {
    PyErr_Print();
    return -1;
  }

  PyCompilerFlags local_flags = _PyCompilerFlags_INIT;
  if (flags == nullptr) {
    flags = &local_flags;
  }
  // TODO(T46358395): Set sys.ps{1,2} in sys module if they don't exist
  int err = 0;
  int ret;
  int nomem_count = 0;
  do {
    ret = PyRun_InteractiveOneObjectEx(fp, filename_str, flags);
    if (ret == -1 && PyErr_Occurred()) {
      // Prevent an endless loop after multiple consecutive MemoryErrors while
      // still allowing an interactive command to fail with a MemoryError.
      if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
        if (++nomem_count > 16) {
          PyErr_Clear();
          err = -1;
          break;
        }
      } else {
        nomem_count = 0;
      }
      PyErr_Print();
      flushStdFiles();
    } else {
      nomem_count = 0;
    }
  } while (ret != E_EOF);
  Py_DECREF(filename_str);
  return err;
}

static void setMainLoader(Thread* thread, Module& module, const char* filename,
                          SymbolId loader_name) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Str filename_str(&scope, runtime->newStrFromCStr(filename));
  Str dunder_main_str(&scope, runtime->symbols()->at(ID(__main__)));
  RawObject loader_obj =
      thread->invokeFunction2(ID(_frozen_importlib_external), loader_name,
                              dunder_main_str, filename_str);
  DCHECK(!loader_obj.isError(), "Unable to call file loader");
  Object loader(&scope, loader_obj);
  moduleAtPutById(thread, module, ID(__loader__), loader);
}

PY_EXPORT int PyRun_SimpleFile(FILE* fp, const char* filename) {
  return PyRun_SimpleFileExFlags(fp, filename, /*closeit=*/0,
                                 /*flags=*/nullptr);
}

PY_EXPORT int PyRun_SimpleFileEx(FILE* fp, const char* filename, int closeit) {
  return PyRun_SimpleFileExFlags(fp, filename, closeit, /*flags=*/nullptr);
}

PY_EXPORT int PyRun_SimpleFileExFlags(FILE* fp, const char* filename,
                                      int closeit, PyCompilerFlags* flags) {
  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  Object module_obj(&scope, runtime->findModuleById(ID(__main__)));
  if (module_obj.isErrorNotFound()) {
    Object name(&scope, runtime->symbols()->at(ID(__main__)));
    module_obj = runtime->newModule(name);
    Object modules(&scope, runtime->modules());
    objectSetItem(thread, modules, name, module_obj);
  }
  Module module(&scope, *module_obj);

  RawObject dunder_file = moduleAtById(thread, module, ID(__file__));
  if (dunder_file.isErrorNotFound()) {
    Str filename_str(&scope, runtime->newStrFromCStr(filename));
    Object cached_obj(&scope, NoneType::object());
    moduleAtPutById(thread, module, ID(__file__), filename_str);
    moduleAtPutById(thread, module, ID(__cached__), cached_obj);
  }

  PyObject* result;
  const char* extension = std::strrchr(filename, '.');
  if (extension != nullptr && std::strcmp(extension, ".pyc") == 0) {
    // Try to run a pyc file
    setMainLoader(thread, module, filename, ID(SourcelessFileLoader));
    result = runPycFile(fp, filename, module, flags);
  } else {
    // When running from stdin, leave __main__.__loader__ alone
    if (std::strcmp(filename, "<stdin>") != 0) {
      setMainLoader(thread, module, filename, ID(SourceFileLoader));
    }
    PyObject* module_proxy =
        ApiHandle::borrowedReference(runtime, module.moduleProxy());
    result = PyRun_FileExFlags(fp, filename, Py_file_input, module_proxy,
                               module_proxy, closeit, flags);
  }
  flushStdFiles();

  int returncode;
  if (result == nullptr) {
    PyErr_Print();
    returncode = -1;
  } else {
    Py_DECREF(result);
    returncode = 0;
  }

  Str dunder_file_name(&scope, runtime->symbols()->at(ID(__file__)));
  RawObject del_result =
      moduleDeleteAttribute(thread, module, dunder_file_name);
  if (del_result.isError()) {
    PyErr_Clear();
  }

  return returncode;
}

PY_EXPORT int PyRun_SimpleString(const char* str) {
  return PyRun_SimpleStringFlags(str, nullptr);
}

PY_EXPORT int PyRun_SimpleStringFlags(const char* str, PyCompilerFlags* flags) {
  PyObject* module = PyImport_AddModule("__main__");
  if (module == nullptr) return -1;
  PyObject* module_proxy = PyModule_GetDict(module);
  PyObject* result =
      PyRun_StringFlags(str, Py_file_input, module_proxy, module_proxy, flags);
  if (result == nullptr) {
    PyErr_Print();
    return -1;
  }
  Py_DECREF(result);
  return 0;
}

PY_EXPORT void PyErr_Display(PyObject* /* exc */, PyObject* value,
                             PyObject* tb) {
  Thread* thread = Thread::current();
  HandleScope scope(thread);

  DCHECK(value != nullptr, "value must be given");
  Object value_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(value)));
  Object tb_obj(&scope, tb ? ApiHandle::asObject(ApiHandle::fromPyObject(tb))
                           : NoneType::object());
  if (displayException(thread, value_obj, tb_obj).isError()) {
    // Don't propagate any exceptions that happened during printing. This isn't
    // great, but it's necessary to match CPython.
    thread->clearPendingException();
  }
}

PY_EXPORT void PyErr_Print() { PyErr_PrintEx(1); }

PY_EXPORT void PyErr_PrintEx(int set_sys_last_vars) {
  Thread* thread = Thread::current();
  if (set_sys_last_vars) {
    printPendingExceptionWithSysLastVars(thread);
  } else {
    printPendingException(thread);
  }
}

PY_EXPORT int PyOS_CheckStack() { UNIMPLEMENTED("PyOS_CheckStack"); }

PY_EXPORT PyObject* PyRun_File(FILE* fp, const char* filename, int start,
                               PyObject* globals, PyObject* locals) {
  return PyRun_FileExFlags(fp, filename, start, globals, locals,
                           /*closeit=*/0, /*flags=*/nullptr);
}

PY_EXPORT PyObject* PyRun_FileEx(FILE* fp, const char* filename, int start,
                                 PyObject* globals, PyObject* locals,
                                 int closeit) {
  return PyRun_FileExFlags(fp, filename, start, globals, locals, closeit,
                           /*flags=*/nullptr);
}

PY_EXPORT PyObject* PyRun_FileExFlags(FILE* fp, const char* filename_cstr,
                                      int start, PyObject* globals,
                                      PyObject* locals, int closeit,
                                      PyCompilerFlags* flags) {
  word file_len;
  unique_c_ptr<byte> buffer(OS::readFile(fp, &file_len));
  if (closeit) std::fclose(fp);

  Thread* thread = Thread::current();
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);

  int iflags = flags != nullptr ? flags->cf_flags : 0;
  // C-API uses this flag but it's an error for managed to pass it in
  iflags &= ~PyCF_SOURCE_IS_UTF8;

  View<byte> data(buffer.get(), file_len);
  Object source(&scope, runtime->newBytesWithAll(data));
  Str filename(&scope, runtime->newStrFromCStr(filename_cstr));
  SymbolId mode_id;
  if (start == Py_single_input) {
    mode_id = ID(single);
  } else if (start == Py_file_input) {
    mode_id = ID(exec);
  } else if (start == Py_eval_input) {
    mode_id = ID(eval);
  } else {
    thread->raiseWithFmt(
        LayoutId::kValueError,
        "mode must be 'Py_single_input', 'Py_file_input' or 'Py_eval_input'");
    return nullptr;
  }
  RawObject code =
      compile(thread, source, filename, mode_id, iflags, Py_OptimizeFlag);
  if (code.isError()) {
    return nullptr;
  }
  Code code_code(&scope, code);
  Object globals_obj(&scope, ApiHandle::asObject(ApiHandle::fromPyObject(globals)));
  Object module_obj(&scope, NoneType::object());
  if (globals_obj.isModuleProxy()) {
    module_obj = ModuleProxy::cast(*globals_obj).module();
  } else if (runtime->isInstanceOfDict(*globals_obj)) {
    UNIMPLEMENTED("User-defined globals is unsupported");
  } else {
    thread->raiseBadInternalCall();
    return nullptr;
  }
  Object implicit_globals(&scope, NoneType::object());
  if (locals != nullptr && globals != locals) {
    implicit_globals = ApiHandle::asObject(ApiHandle::fromPyObject(locals));
    if (!runtime->isMapping(thread, implicit_globals)) {
      thread->raiseBadInternalCall();
      return nullptr;
    }
  }
  Module module(&scope, *module_obj);
  RawObject result = thread->exec(code_code, module, implicit_globals);
  return result.isError() ? nullptr : ApiHandle::newReference(runtime, result);
}

PY_EXPORT PyObject* PyRun_FileFlags(FILE* fp, const char* filename, int start,
                                    PyObject* globals, PyObject* locals,
                                    PyCompilerFlags* flags) {
  return PyRun_FileExFlags(fp, filename, start, globals, locals,
                           /*closeit=*/0, flags);
}

PY_EXPORT PyObject* PyRun_String(const char* str, int start, PyObject* globals,
                                 PyObject* locals) {
  return PyRun_StringFlags(str, start, globals, locals, /*flags=*/nullptr);
}

PY_EXPORT PyObject* PyRun_StringFlags(const char* str, int start,
                                      PyObject* globals, PyObject* locals,
                                      PyCompilerFlags* flags) {
  Thread* thread = Thread::current();
  PyObject* filename = ApiHandle::borrowedReference(
      thread->runtime(), Runtime::internStrFromCStr(thread, "<string>"));

  PyArena* arena = PyArena_New();
  if (arena == nullptr) return nullptr;

  struct _mod* mod =
      PyParser_ASTFromStringObject(str, filename, start, flags, arena);
  PyObject* ret = nullptr;
  if (mod != nullptr) {
    ret = runMod(mod, filename, globals, locals, flags, arena);
  }
  PyArena_Free(arena);
  return ret;
}

}  // namespace py
