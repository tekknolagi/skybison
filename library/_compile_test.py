#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import io
import unittest
from types import CodeType

try:
    from _compiler_opcode import opcode
except ImportError:
    import opcode
from test_support import pyro_only


def _dis_instruction(opcode, code: CodeType, op: int, oparg: int):  # noqa: C901
    result = opcode.opname[op]
    if op < opcode.HAVE_ARGUMENT and oparg == 0:
        oparg_str = None
    elif op in opcode.hasconst:
        cnst = code.co_consts[oparg]
        co_name = getattr(cnst, "co_name", None)
        if co_name:
            oparg_str = f"{oparg}:code object {co_name}"
        else:
            oparg_str = repr(code.co_consts[oparg])
    elif op in opcode.hasname:
        oparg_str = code.co_names[oparg]
    elif op in opcode.haslocal:
        oparg_str = code.co_varnames[oparg]
    elif op in opcode.hascompare:
        oparg_str = opcode.CMP_OP[oparg]
    elif result == "FORMAT_VALUE":
        fvc = oparg & opcode.FVC_MASK
        if fvc == opcode.FVC_STR:
            oparg_str = "str"
        elif fvc == opcode.FVC_REPR:
            oparg_str = "repr"
        elif fvc == opcode.FVC_ASCII:
            oparg_str = "ascii"
        else:
            assert fvc == opcode.FVC_NONE
            oparg_str = None
        if (oparg & opcode.FVS_HAVE_SPEC) != 0:
            oparg_str = "" if oparg_str is None else (oparg_str + " ")
            oparg_str += "have_spec"
    else:
        oparg_str = str(oparg)
    if oparg_str is not None:
        result = f"{result} {oparg_str}"
    return result


def _dis_code(opcode, lines, code: CodeType):
    bytecode = code.co_code
    bytecode_len = len(bytecode)
    i = 0
    while i < bytecode_len:
        op = bytecode[i]
        oparg = bytecode[i + 1]
        i += opcode.CODEUNIT_SIZE
        while op == opcode.EXTENDED_ARG:
            op = bytecode[i]
            oparg = oparg << 8 | bytecode[i + 1]
            i += opcode.CODEUNIT_SIZE
        lines.append(_dis_instruction(opcode, code, op, oparg))
    lines.append("")
    for idx, cnst in enumerate(code.co_consts):
        co_code = getattr(cnst, "co_code", None)
        if co_code:
            lines.append(f"# {idx}:code object {cnst.co_name}")
            _dis_code(opcode, lines, cnst)


def simpledis(opcode, code: CodeType):
    """Simple disassembler, showing only opcode names + args.
    Compare to the `dis` module: This producers simpler output making it easier
    to match in tests. The `opcode` module is parameterized."""
    lines = []
    _dis_code(opcode, lines, code)
    return "\n".join(lines)


def test_compile(source, filename="<test>", mode="eval", flags=0, optimize=True):
    return compile(source, filename, mode, flags, None, optimize)


def compile_function(source, func_name):
    module_code = test_compile(source, mode="exec")
    globals = {}
    locals = {}
    exec(module_code, globals, locals)
    return locals[func_name]


def dis(code):
    return simpledis(opcode, code)


@pyro_only
class StrModOptimizer(unittest.TestCase):
    def test_constant_folds(self):
        source = "'%s %% foo %r bar %a %s' % (1, 'baz', 3, 4)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST "1 % foo 'baz' bar 3 4"
RETURN_VALUE
""",
        )

    def test_empty_args(self):
        source = "'foo' % ()"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST 'foo'
RETURN_VALUE
""",
        )

    @pyro_only
    def test_empty_args_executes(self):
        source = "'foo' % ()"
        self.assertEqual(eval(test_compile(source)), "foo")  # noqa: P204

    def test_percent_a_uses_format_value(self):
        source = "'%a' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME x
FORMAT_VALUE ascii
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_a_executes(self):
        source = "'%a' % (x,)"
        self.assertEqual(
            eval(test_compile(source), {"x": "\u1234"}), "'\\u1234'"  # noqa: P204
        )

    def test_percent_percent(self):
        source = "'T%%est' % ()"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST 'T%est'
RETURN_VALUE
""",
        )

    def test_percent_r_uses_format_value(self):
        source = "'%r' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME x
FORMAT_VALUE repr
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_r_executes(self):
        source = "'%r' % (x,)"
        self.assertEqual(
            eval(test_compile(source), {"x": "bar"}), "'bar'"  # noqa: P204
        )

    def test_percent_s_uses_format_value(self):
        source = "'%s' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME x
FORMAT_VALUE str
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_s_executes(self):
        source = "'%s' % (x,)"
        self.assertEqual(eval(test_compile(source), {"x": "bar"}), "bar")  # noqa: P204

    def test_percent_s_with_width_uses_format_value(self):
        source = "'%13s' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME x
LOAD_CONST '>13'
FORMAT_VALUE str have_spec
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_s_with_width_executes(self):
        source = "'%7s' % (x,)"
        self.assertEqual(
            eval(test_compile(source), {"x": 4231}), "   4231"  # noqa: P204
        )

    def test_percent_d_i_u_calls_convert_and_formats_value(self):
        expected = """\
LOAD_CONST ''
LOAD_METHOD _mod_convert_number_int
LOAD_NAME x
CALL_METHOD 1
FORMAT_VALUE
RETURN_VALUE
"""
        source0 = "'%d' % (x,)"
        source1 = "'%i' % (x,)"
        source2 = "'%u' % (x,)"
        self.assertEqual(dis(test_compile(source0)), expected)
        self.assertEqual(dis(test_compile(source1)), expected)
        self.assertEqual(dis(test_compile(source2)), expected)

    @pyro_only
    def test_percent_d_i_u_executes(self):
        class C:
            def __index__(self):
                raise Exception("should not be called")

            def __int__(self):
                return 13

        source0 = "'%d' % (x,)"
        source1 = "'%i' % (x,)"
        source2 = "'%u' % (x,)"
        self.assertEqual(eval(test_compile(source0), {"x": -42}), "-42")  # noqa: P204
        self.assertEqual(
            eval(test_compile(source1), {"x": 0x1234}), "4660"  # noqa: P204
        )
        self.assertEqual(eval(test_compile(source2), {"x": C()}), "13")  # noqa: P204

    @pyro_only
    def test_percent_d_raises_type_error(self):
        class C:
            pass

        source = "'%d' % (x,)"
        code = test_compile(source)
        with self.assertRaisesRegex(TypeError, "format requires a number, not C"):
            eval(code, None, {"x": C()})  # noqa: P204

    def test_percent_i_with_width_and_zero_uses_format_value_with_spec(self):
        source = "'%05d' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST ''
LOAD_METHOD _mod_convert_number_int
LOAD_NAME x
CALL_METHOD 1
LOAD_CONST '05'
FORMAT_VALUE have_spec
RETURN_VALUE
""",
        )

    def test_percent_o_calls_convert_and_formats_value(self):
        source = "'%5o' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST ''
LOAD_METHOD _mod_convert_number_index
LOAD_NAME x
CALL_METHOD 1
LOAD_CONST '5o'
FORMAT_VALUE have_spec
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_o_executes(self):
        class C:
            def __index__(self):
                return -93

            def __int__(self):
                raise Exception("should not be called")

        source = "'%5o' % (x,)"
        self.assertEqual(eval(test_compile(source), {"x": C()}), " -135")  # noqa: P204

    def test_percent_x_calls_convert_and_formats_value(self):
        source = "'%03x' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST ''
LOAD_METHOD _mod_convert_number_index
LOAD_NAME x
CALL_METHOD 1
LOAD_CONST '03x'
FORMAT_VALUE have_spec
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_x_executes(self):
        class C:
            def __index__(self):
                return -11

            def __int__(self):
                raise Exception("should not be called")

        source = "'%04x' % (x,)"
        self.assertEqual(eval(test_compile(source), {"x": C()}), "-00b")  # noqa: P204

    def test_percent_X_calls_convert_and_formats_value(self):
        source = "'%4X' % (x,)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST ''
LOAD_METHOD _mod_convert_number_index
LOAD_NAME x
CALL_METHOD 1
LOAD_CONST '4X'
FORMAT_VALUE have_spec
RETURN_VALUE
""",
        )

    @pyro_only
    def test_percent_X_executes(self):
        class C:
            def __index__(self):
                return 51966

            def __int__(self):
                raise Exception("should not be called")

        source = "'%04X' % (x,)"
        self.assertEqual(eval(test_compile(source), {"x": C()}), "CAFE")  # noqa: P204

    def test_mixed_format_uses_format_value_and_build_string(self):
        source = "'%s %% foo %r bar %a %s' % (a, b, c, d)"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME a
FORMAT_VALUE str
LOAD_CONST ' '
LOAD_CONST '% foo '
LOAD_NAME b
FORMAT_VALUE repr
LOAD_CONST ' bar '
LOAD_NAME c
FORMAT_VALUE ascii
LOAD_CONST ' '
LOAD_NAME d
FORMAT_VALUE str
BUILD_STRING 8
RETURN_VALUE
""",
        )

    @pyro_only
    def test_mixed_format_executes(self):
        source = "'%s %% foo %r bar %a %s' % (a, b, c, d)"
        locals = {"a": 1, "b": 2, "c": 3, "d": 4}
        self.assertEqual(
            eval(test_compile(source), locals), "1 % foo 2 bar 3 4"  # noqa: P204
        )

    def test_no_tuple_arg_calls_check_single_arg(self):
        source = "'%s' % x"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_CONST ''
LOAD_METHOD _mod_check_single_arg
LOAD_NAME x
CALL_METHOD 1
LOAD_CONST 0
BINARY_SUBSCR
FORMAT_VALUE str
RETURN_VALUE
""",
        )

    @pyro_only
    def test_no_tuple_arg_executes(self):
        source = "'%s' % x"
        self.assertEqual(eval(test_compile(source), {"x": 5.5}), "5.5")  # noqa: P204

    @pyro_only
    def test_no_tuple_raises_type_error_not_enough_arguments(self):
        source = "'%s' % x"
        with self.assertRaisesRegex(
            TypeError, "not enough arguments for format string"
        ):
            eval(test_compile(source), {"x": ()})  # noqa: P204

    @pyro_only
    def test_no_tuple_raises_type_error_not_all_converted(self):
        source = "'%s' % x"
        with self.assertRaisesRegex(
            TypeError, "not all arguments converted during string formatting"
        ):
            eval(test_compile(source), {"x": (1, 2)})  # noqa: P204

    def test_trailing_percent_it_not_optimized(self):
        source = "'foo%' % (x,)"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))

    def test_multiple_specs_without_tuple_is_not_optimized(self):
        source = "'%s%s' % x"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))

    def test_spec_with_mapping_key_is_not_optimized(self):
        source = "'%(%s)' % x"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))

    def test_with_too_small_arg_tuple_is_not_optimized(self):
        source = "'%s%s%s' % (x,y)"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))

    def test_with_too_large_arg_tuple_is_not_optimized(self):
        source = "'%s%s%s' % (w,x,y,z)"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))

    def test_with_unknown_specifier_is_not_optimized(self):
        source = "'%Z' % (x,)"
        self.assertIn("BINARY_MODULO", dis(test_compile(source)))


@pyro_only
class CustomOpcodeTests(unittest.TestCase):
    def test_is_generates_COMPARE_IS(self):
        source = "a is b"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME a
LOAD_NAME b
COMPARE_IS 0
RETURN_VALUE
""",
        )

    def test_is_not_generates_COMPARE_IS_NOT(self):
        source = "a is not b"
        self.assertEqual(
            dis(test_compile(source)),
            """\
LOAD_NAME a
LOAD_NAME b
COMPARE_IS_NOT 0
RETURN_VALUE
""",
        )


@pyro_only
class InlineComprehensionTests(unittest.TestCase):
    def test_list_comprehension_code_is_inline(self):
        source = "[x for x in y]"
        self.assertEqual(
            dis(test_compile(source)),
            """\
BUILD_LIST 0
LOAD_NAME y
GET_ITER
FOR_ITER 8
STORE_NAME _gen$x
LOAD_NAME _gen$x
LIST_APPEND 2
JUMP_ABSOLUTE 6
RETURN_VALUE
""",
        )

    @pyro_only
    def test_list_comprehension_code_executes(self):
        source = "[x for x in y]"
        self.assertEqual(
            eval(test_compile(source), {"y": range(4)}), [0, 1, 2, 3]  # noqa: P204
        )

    def test_set_comprehension_code_is_inline(self):
        source = "{x for x in y}"
        self.assertEqual(
            dis(test_compile(source)),
            """\
BUILD_SET 0
LOAD_NAME y
GET_ITER
FOR_ITER 8
STORE_NAME _gen$x
LOAD_NAME _gen$x
SET_ADD 2
JUMP_ABSOLUTE 6
RETURN_VALUE
""",
        )

    @pyro_only
    def test_set_comprehension_code_executes(self):
        source = "{x for x in y}"
        self.assertEqual(
            eval(test_compile(source), {"y": range(4)}), {0, 1, 2, 3}  # noqa: P204
        )

    def test_dict_comprehension_code_is_inline(self):
        source = "{x:-x for x in y}"
        self.assertEqual(
            dis(test_compile(source)),
            """\
BUILD_MAP 0
LOAD_NAME y
GET_ITER
FOR_ITER 12
STORE_NAME _gen$x
LOAD_NAME _gen$x
LOAD_NAME _gen$x
UNARY_NEGATIVE
MAP_ADD 2
JUMP_ABSOLUTE 6
RETURN_VALUE
""",
        )

    @pyro_only
    def test_dict_comprehension_code_executes(self):
        source = "{x:-x for x in y}"
        self.assertEqual(
            eval(test_compile(source), {"y": range(4)}),  # noqa: P204
            {0: 0, 1: -1, 2: -2, 3: -3},
        )

    def test_nested_comprehension_in_iterator_code_is_inline(self):
        source = "[-x for x in [x for x in x if x > 3]]"
        self.assertEqual(
            (dis(test_compile(source))),
            """\
BUILD_LIST 0
BUILD_LIST 0
LOAD_NAME x
GET_ITER
FOR_ITER 16
STORE_NAME _gen$x
LOAD_NAME _gen$x
LOAD_CONST 3
COMPARE_OP >
POP_JUMP_IF_FALSE 8
LOAD_NAME _gen$x
LIST_APPEND 2
JUMP_ABSOLUTE 8
GET_ITER
FOR_ITER 10
STORE_NAME _gen1$x
LOAD_NAME _gen1$x
UNARY_NEGATIVE
LIST_APPEND 2
JUMP_ABSOLUTE 28
RETURN_VALUE
""",
        )

    @pyro_only
    def test_nested_comprehension_in_iterator_executes(self):
        source = "[-x for x in [x for x in x if x > 3]]"
        self.assertEqual(
            eval(test_compile(source), {"x": range(8)}), [-4, -5, -6, -7]  # noqa: P204
        )

    def test_nested_comprehension_in_element_code_is_inline(self):
        source = "[[x for x in range(x) if x & 1 == 1] for x in x]"
        self.assertEqual(
            (dis(test_compile(source))),
            """\
BUILD_LIST 0
LOAD_NAME x
GET_ITER
FOR_ITER 38
STORE_NAME _gen$x
BUILD_LIST 0
LOAD_NAME range
LOAD_NAME _gen$x
CALL_FUNCTION 1
GET_ITER
FOR_ITER 20
STORE_NAME _gen1$_gen$x
LOAD_NAME _gen1$_gen$x
LOAD_CONST 1
BINARY_AND
LOAD_CONST 1
COMPARE_OP ==
POP_JUMP_IF_FALSE 20
LOAD_NAME _gen1$_gen$x
LIST_APPEND 2
JUMP_ABSOLUTE 20
LIST_APPEND 2
JUMP_ABSOLUTE 6
RETURN_VALUE
""",
        )

    @pyro_only
    def test_nested_comprehension_in_element_executes(self):
        source = "[[x for x in range(x) if x & 1 == 1] for x in x]"
        self.assertEqual(
            eval(test_compile(source), {"x": range(5)}),  # noqa: P204
            [[], [], [1], [1], [1, 3]],
        )

    def test_nested_lambda_in_comprehension_has_arg_renamed(self):
        source = "[lambda x: x for x in r]"
        self.assertEqual(
            (dis(test_compile(source))),
            """\
BUILD_LIST 0
LOAD_NAME r
GET_ITER
FOR_ITER 12
STORE_NAME _gen$x
LOAD_CONST 0:code object <lambda>
LOAD_CONST '<lambda>'
MAKE_FUNCTION 0
LIST_APPEND 2
JUMP_ABSOLUTE 6
RETURN_VALUE

# 0:code object <lambda>
LOAD_FAST_UNCHECKED _gen$x
RETURN_VALUE
""",
        )

    @pyro_only
    def test_nested_lambda_in_comprehension_executes(self):
        source = "[lambda x: x for x in r]"
        lambdas = eval(test_compile(source), {"r": range(3)})  # noqa: P204
        self.assertEqual(lambdas[0](22), 22)
        self.assertEqual(lambdas[1]("foo"), "foo")
        self.assertEqual(lambdas[2](int), int)

    def test_multiple_comprehensions_use_unique_prefixes(self):
        source = "[lambda: x for x in r], [x for x in (42,)]"
        self.assertEqual(
            (dis(test_compile(source))),
            """\
BUILD_LIST 0
LOAD_NAME r
GET_ITER
FOR_ITER 12
STORE_NAME _gen$x
LOAD_CONST 0:code object <lambda>
LOAD_CONST '<lambda>'
MAKE_FUNCTION 0
LIST_APPEND 2
JUMP_ABSOLUTE 6
BUILD_LIST 0
LOAD_CONST (42,)
GET_ITER
FOR_ITER 8
STORE_NAME _gen1$x
LOAD_NAME _gen1$x
LIST_APPEND 2
JUMP_ABSOLUTE 26
BUILD_TUPLE 2
RETURN_VALUE

# 0:code object <lambda>
LOAD_GLOBAL _gen$x
RETURN_VALUE
""",
        )

    @pyro_only
    def test_multiple_comprehensions_executes(self):
        source = "[lambda: x for x in r], [x for x in (42,)]"
        lambdas, dummy = eval(test_compile(source), {"r": range(13, 15)})  # noqa: P204
        self.assertEqual(lambdas[0](), 14)
        self.assertEqual(lambdas[1](), 14)

    def test_renamer_with_locals_in_list_comprehension_does_not_rename(self):
        iterable = [1]
        result = [locals().keys() for item in iterable][0]
        self.assertEqual(sorted(result), [".0", "item"])

    def test_renamer_with_locals_in_dict_comprehension_does_not_rename(self):
        iterable = [1]
        result = {"key": locals().keys() for item in iterable}["key"]
        self.assertEqual(sorted(result), [".0", "item"])

    def test_renamer_with_locals_in_set_comprehension_does_not_rename(self):
        iterable = [1]
        result = {locals().keys() for item in iterable}.pop()
        self.assertEqual(sorted(result), [".0", "item"])

    def test_renamer_with_locals_in_gen_comprehension_does_not_rename(self):
        iterable = [1]
        result = tuple(locals().keys() for item in iterable)[0]
        self.assertEqual(sorted(result), [".0", "item"])


@pyro_only
class DefiniteAsssignmentAnalysisTests(unittest.TestCase):
    def test_load_parameter_is_unchecked(self):
        source = """
def foo(x):
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(123), 123)

    def test_load_kwonly_parameter_is_unchecked(self):
        source = """
def foo(*, x):
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(x=123), 123)

    def test_load_varargs_parameter_is_unchecked(self):
        source = """
def foo(*x):
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(123), (123,))

    def test_load_varkeyargs_parameter_is_unchecked(self):
        source = """
def foo(**x):
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(a=123), {"a": 123})

    def test_load_parameter_with_del_is_checked(self):
        source = """
def foo(x):
    del x
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST x
LOAD_FAST x
RETURN_VALUE
""",
        )
        with self.assertRaisesRegex(
            UnboundLocalError, "local variable 'x' referenced before assignment"
        ):
            func(123)

    def test_load_parameter_with_del_after_is_unchecked(self):
        source = """
def foo(x):
    y = x
    del x
    return y
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED x
STORE_FAST y
DELETE_FAST x
LOAD_FAST_UNCHECKED y
RETURN_VALUE
""",
        )
        self.assertEqual(func(123), 123)

    def test_variable_defined_in_local_is_unchecked(self):
        source = """
def foo():
    x = 3
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_CONST 3
STORE_FAST x
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 3)

    def test_variable_defined_in_one_branch_is_checked(self):
        source = """
def foo(cond):
    if cond:
        x = 3
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST_UNCHECKED x
LOAD_FAST_UNCHECKED cond
POP_JUMP_IF_FALSE 10
LOAD_CONST 3
STORE_FAST x
LOAD_FAST x
RETURN_VALUE
""",
        )
        self.assertEqual(func(True), 3)
        with self.assertRaisesRegex(
            UnboundLocalError, "local variable 'x' referenced before assignment"
        ):
            func(False)

    def test_variable_defined_in_one_branch_if_else_is_checked(self):
        source = """
def foo(cond):
    if cond:
        x = 3
    else:
        pass
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST_UNCHECKED x
LOAD_FAST_UNCHECKED cond
POP_JUMP_IF_FALSE 12
LOAD_CONST 3
STORE_FAST x
JUMP_FORWARD 0
LOAD_FAST x
RETURN_VALUE
""",
        )
        self.assertEqual(func(True), 3)
        with self.assertRaisesRegex(
            UnboundLocalError, "local variable 'x' referenced before assignment"
        ):
            func(False)

    def test_variable_defined_in_both_branches_if_else_is_unchecked(self):
        source = """
def foo(cond):
    if cond:
        x = 3
    else:
        x = 4
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_FAST_UNCHECKED cond
POP_JUMP_IF_FALSE 10
LOAD_CONST 3
STORE_FAST x
JUMP_FORWARD 4
LOAD_CONST 4
STORE_FAST x
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(True), 3)
        self.assertEqual(func(False), 4)

    def test_loop_is_unchecked(self):
        source = """
def foo(cond):
    while True:
        print(cond)
"""
        self.assertEqual(
            dis(compile_function(source, "foo").__code__),
            """\
LOAD_GLOBAL print
LOAD_FAST_UNCHECKED cond
CALL_FUNCTION 1
POP_TOP
JUMP_ABSOLUTE 0
LOAD_CONST None
RETURN_VALUE
""",
        )

    def test_loop_with_del_is_checked(self):
        source = """
def foo(cond):
    while True:
        1 + cond  # use it
        del cond
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_CONST 1
LOAD_FAST cond
BINARY_ADD
POP_TOP
DELETE_FAST cond
JUMP_ABSOLUTE 0
LOAD_CONST None
RETURN_VALUE
""",
        )
        with self.assertRaisesRegex(
            UnboundLocalError, "local variable 'cond' referenced before assignment"
        ):
            func(True)

    def test_loop_variable_in_for_loop_is_unchecked(self):
        source = """
def foo(n):
    result = []
    for i in range(n):
        result.append(i)
    return result
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
BUILD_LIST 0
STORE_FAST result
LOAD_GLOBAL range
LOAD_FAST_UNCHECKED n
CALL_FUNCTION 1
GET_ITER
FOR_ITER 14
STORE_FAST i
LOAD_FAST_UNCHECKED result
LOAD_METHOD append
LOAD_FAST_UNCHECKED i
CALL_METHOD 1
POP_TOP
JUMP_ABSOLUTE 12
LOAD_FAST_UNCHECKED result
RETURN_VALUE
""",
        )
        self.assertEqual(func(3), [0, 1, 2])

    def test_loop_variable_after_for_loop_is_checked(self):
        source = """
def foo(n):
    for i in range(n):
        pass
    return i
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST_UNCHECKED i
LOAD_GLOBAL range
LOAD_FAST_UNCHECKED n
CALL_FUNCTION 1
GET_ITER
FOR_ITER 4
STORE_FAST i
JUMP_ABSOLUTE 10
LOAD_FAST i
RETURN_VALUE
""",
        )
        self.assertEqual(func(3), 2)

    def test_try_with_use_in_try_is_unchecked(self):
        source = """
def foo():
    try:
        x = 123
        return x
    except:
        pass
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
SETUP_FINALLY 10
LOAD_CONST 123
STORE_FAST x
LOAD_FAST_UNCHECKED x
POP_BLOCK
RETURN_VALUE
POP_TOP
POP_TOP
POP_TOP
POP_EXCEPT
JUMP_FORWARD 2
END_FINALLY
LOAD_CONST None
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 123)

    @unittest.skip(
        "TODO(emacs): This should be unchecked but we need to tail-duplicate finally blocks"
    )
    def test_try_with_use_in_else_is_unchecked(self):
        source = """
def foo():
    try:
        x = 123
    except:
        pass
    else:
        return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
SETUP_FINALLY 8
LOAD_CONST 123
STORE_FAST x
POP_BLOCK
JUMP_FORWARD 12
POP_TOP
POP_TOP
POP_TOP
POP_EXCEPT
JUMP_FORWARD 6
END_FINALLY
LOAD_FAST_UNCHECKED x
RETURN_VALUE
LOAD_CONST None
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 123)

    def test_try_with_use_after_except_is_checked(self):
        source = """
def foo():
    try:
        x = 123
    except:
        pass
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST_UNCHECKED x
SETUP_FINALLY 8
LOAD_CONST 123
STORE_FAST x
POP_BLOCK
JUMP_FORWARD 12
POP_TOP
POP_TOP
POP_TOP
POP_EXCEPT
JUMP_FORWARD 2
END_FINALLY
LOAD_FAST x
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 123)

    def test_try_with_use_in_except_is_checked(self):
        source = """
def foo():
    try:
        x = 123
    except:
        return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
DELETE_FAST_UNCHECKED x
SETUP_FINALLY 8
LOAD_CONST 123
STORE_FAST x
POP_BLOCK
JUMP_FORWARD 16
POP_TOP
POP_TOP
POP_TOP
LOAD_FAST x
ROT_FOUR
POP_EXCEPT
RETURN_VALUE
END_FINALLY
LOAD_CONST None
RETURN_VALUE
""",
        )
        self.assertEqual(func(), None)

    def test_try_with_use_in_finally_is_unchecked(self):
        source = """
def foo():
    try:
        x = 123
    finally:
        return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_CONST None
SETUP_FINALLY 8
LOAD_CONST 123
STORE_FAST x
POP_BLOCK
BEGIN_FINALLY
LOAD_FAST_UNCHECKED x
POP_FINALLY 1
ROT_TWO
POP_TOP
RETURN_VALUE
END_FINALLY
POP_TOP
""",
        )
        self.assertEqual(func(), 123)

    def test_try_with_use_after_finally_is_unchecked(self):
        source = """
def foo():
    try:
        x = 123
    finally:
        x = 456
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
SETUP_FINALLY 8
LOAD_CONST 123
STORE_FAST x
POP_BLOCK
BEGIN_FINALLY
LOAD_CONST 456
STORE_FAST x
END_FINALLY
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 456)

    def test_try_with_use_after_finally_is_unchecked_2(self):
        source = """
def foo():
    try:
        pass
    finally:
        x = 456
    return x
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
SETUP_FINALLY 4
POP_BLOCK
BEGIN_FINALLY
LOAD_CONST 456
STORE_FAST x
END_FINALLY
LOAD_FAST_UNCHECKED x
RETURN_VALUE
""",
        )
        self.assertEqual(func(), 456)

    def test_use_in_with_is_unchecked(self):
        source = """
class CM(object):
    def __init__(self):
        self.state = "init"
    def __enter__(self):
        self.state = "enter"
    def __exit__(self, type, value, traceback):
        self.state = "exit"

def foo():
    with CM() as cm:
        return cm
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_GLOBAL CM
CALL_FUNCTION 0
SETUP_WITH 18
STORE_FAST cm
LOAD_FAST_UNCHECKED cm
POP_BLOCK
ROT_TWO
BEGIN_FINALLY
WITH_CLEANUP_START
WITH_CLEANUP_FINISH
POP_FINALLY 0
RETURN_VALUE
WITH_CLEANUP_START
WITH_CLEANUP_FINISH
END_FINALLY
LOAD_CONST None
RETURN_VALUE
""",
        )
        # TODO(emacs): Figure out how to get the NameError for CM to go away.
        return
        self.assertEqual(func(), 456)

    def test_use_after_with_is_unchecked(self):
        source = """
class CM(object):
    def __init__(self):
        self.state = "init"
    def __enter__(self):
        self.state = "enter"
    def __exit__(self, type, value, traceback):
        self.state = "exit"

def foo():
    with CM() as cm:
        pass
    return cm
"""
        func = compile_function(source, "foo")
        self.assertEqual(
            dis(func.__code__),
            """\
LOAD_GLOBAL CM
CALL_FUNCTION 0
SETUP_WITH 6
STORE_FAST cm
POP_BLOCK
BEGIN_FINALLY
WITH_CLEANUP_START
WITH_CLEANUP_FINISH
END_FINALLY
LOAD_FAST_UNCHECKED cm
RETURN_VALUE
""",
        )
        # TODO(emacs): Figure out how to get the NameError for CM to go away.
        return
        self.assertEqual(func(), 456)

#     def test_use_in_yield_is_unchecked(self):
#         source = """
# async def foo():
#     async for i in range(3):
#         yield i
# """
#         func = compile_function(source, "foo")
#         self.assertEqual(
#             dis(func.__code__),
#             """\
# """,
#         )
#         # TODO(emacs): Figure out how to get the NameError for CM to go away.
#         return
#         self.assertEqual(list(func()), [0, 1, 2])

    # TODO(emacs): Test with (multiple context managers)
    # TODO(emacs): Test async with
    # TODO(emacs): Test async loops


if __name__ == "__main__":
    unittest.main()
