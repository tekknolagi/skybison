# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import ast
from ast import AST
from compiler import compile as compiler_compile
from compiler.optimizer import BIN_OPS, is_const, get_const_value
from compiler.py38.optimizer import AstOptimizer38
from compiler.pyassem import PyFlowGraph38
from compiler.pycodegen import Python38CodeGenerator
from compiler.symbols import SymbolVisitor
from compiler.visitor import ASTVisitor, walk
from compiler.consts import CO_VARARGS, CO_VARKEYWORDS

import _compiler_opcode as opcodepyro

CODEUNIT_SIZE = opcodepyro.opcode.CODEUNIT_SIZE


def should_rewrite_printf(node):
    return isinstance(node.left, ast.Str) and isinstance(node.op, ast.Mod)


def create_conversion_call(name, value):
    method = ast.Attribute(ast.Str(""), name, ast.Load())
    return ast.Call(method, args=[value], keywords=[])


def try_constant_fold_mod(format_string, right):
    r = get_const_value(right)
    return ast.Str(format_string.__mod__(r))


class AstOptimizerPyro(AstOptimizer38):
    def rewrite_str_mod(self, left, right):  # noqa: C901
        format_string = left.s
        try:
            if is_const(right):
                return try_constant_fold_mod(format_string, right)
            # Try and collapse the whole expression into a string
            const_tuple = self.makeConstTuple(right.elts)
            if const_tuple:
                return ast.Str(format_string.__mod__(const_tuple.value))
        except Exception:
            pass
        n_specifiers = 0
        i = 0
        length = len(format_string)
        while i < length:
            i = format_string.find("%", i)
            if i == -1:
                break
            ch = format_string[i]
            i += 1

            if i >= length:
                # Invalid format string ending in a single percent
                return None
            ch = format_string[i]
            i += 1
            if ch == "%":
                # Break the string apart at '%'
                continue
            elif ch == "(":
                # We don't support dict lookups and may get confused from
                # inner '%' chars
                return None
            n_specifiers += 1

        rhs = right
        if isinstance(right, ast.Tuple):
            rhs_values = rhs.elts
            num_values = len(rhs_values)
        else:
            # If RHS is not a tuple constructor, then we only support the
            # situation with a single format specifier in the string, by
            # normalizing `rhs` to a one-element tuple:
            # `_mod_check_single_arg(rhs)[0]`
            rhs_values = None
            if n_specifiers != 1:
                return None
            num_values = 1
        i = 0
        value_idx = 0
        segment_begin = 0
        strings = []
        while i < length:
            i = format_string.find("%", i)
            if i == -1:
                break
            ch = format_string[i]
            i += 1

            segment_end = i - 1
            if segment_end - segment_begin > 0:
                substr = format_string[segment_begin:segment_end]
                strings.append(ast.Str(substr))

            if i >= length:
                return None
            ch = format_string[i]
            i += 1

            # Parse flags and width
            spec_begin = i - 1
            have_width = False
            while True:
                if ch == "0":
                    # TODO(matthiasb): Support ' ', '+', '#', etc
                    # They mostly have the same meaning. However they can
                    # appear in any order here but must follow stricter
                    # conventions in f-strings.
                    if i >= length:
                        return None
                    ch = format_string[i]
                    i += 1
                    continue
                break
            if "1" <= ch <= "9":
                have_width = True
                if i >= length:
                    return None
                ch = format_string[i]
                i += 1
                while "0" <= ch <= "9":
                    if i >= length:
                        return None
                    ch = format_string[i]
                    i += 1
            spec_str = ""
            if i - 1 - spec_begin > 0:
                spec_str = format_string[spec_begin : i - 1]

            if ch == "%":
                # Handle '%%'
                segment_begin = i - 1
                continue

            # Handle remaining supported cases that use a value from RHS
            if rhs_values is not None:
                if value_idx >= num_values:
                    return None
                value = rhs_values[value_idx]
            else:
                # We have a situation like `"%s" % x` without tuple on RHS.
                # Transform to: f"{''._mod_check_single_arg(x)[0]}"
                converted = create_conversion_call("_mod_check_single_arg", rhs)
                value = ast.Subscript(converted, ast.Index(ast.Num(0)), ast.Load())
            value_idx += 1

            if ch in "sra":
                # Rewrite "%s" % (x,) to f"{x!s}"
                if have_width:
                    # Need to explicitly specify alignment because `%5s`
                    # aligns right, while `f"{x:5}"` aligns left.
                    spec_str = ">" + spec_str
                format_spec = ast.Str(spec_str) if spec_str else None
                formatted = ast.FormattedValue(value, ord(ch), format_spec)
                strings.append(formatted)
            elif ch in "diu":
                # Rewrite "%d" % (x,) to f"{''._mod_convert_number_int(x)}".
                # Calling a method on the empty string is a hack to access a
                # well-known function regardless of the surrounding
                # environment.
                converted = create_conversion_call("_mod_convert_number_int", value)
                format_spec = ast.Str(spec_str) if spec_str else None
                formatted = ast.FormattedValue(converted, -1, format_spec)
                strings.append(formatted)
            elif ch in "xXo":
                # Rewrite "%x" % (v,) to f"{''._mod_convert_number_index(v):x}".
                # Calling a method on the empty string is a hack to access a
                # well-known function regardless of the surrounding
                # environment.
                converted = create_conversion_call("_mod_convert_number_index", value)
                format_spec = ast.Str(spec_str + ch)
                formatted = ast.FormattedValue(converted, -1, format_spec)
                strings.append(formatted)
            else:
                return None
            # Begin next segment after specifier
            segment_begin = i

        if value_idx != num_values:
            return None

        segment_end = length
        if segment_end - segment_begin > 0:
            substr = format_string[segment_begin:segment_end]
            strings.append(ast.Str(substr))

        return ast.JoinedStr(strings)

    def visitBinOp(self, node: ast.BinOp) -> ast.expr:
        left = self.visit(node.left)
        right = self.visit(node.right)

        if is_const(left) and is_const(right):
            handler = BIN_OPS.get(type(node.op))
            if handler is not None:
                lval = get_const_value(left)
                rval = get_const_value(right)
                try:
                    return ast.copy_location(ast.Constant(handler(lval, rval)), node)
                except Exception:
                    pass

        if should_rewrite_printf(node):
            result = self.rewrite_str_mod(left, right)
            if result:
                return self.visit(result)

        return self.update_node(node, left=left, right=right)


class BytecodeOp:
    def __init__(self, op: int, arg: int, idx: int) -> None:
        self.op = op
        self.arg = arg
        self.idx = idx

    def is_branch(self) -> bool:
        return self.op in {
            opcodepyro.opcode.FOR_ITER,
            opcodepyro.opcode.JUMP_ABSOLUTE,
            opcodepyro.opcode.JUMP_FORWARD,
            opcodepyro.opcode.JUMP_IF_FALSE_OR_POP,
            opcodepyro.opcode.JUMP_IF_TRUE_OR_POP,
            opcodepyro.opcode.POP_JUMP_IF_FALSE,
            opcodepyro.opcode.POP_JUMP_IF_TRUE,
        }

    def is_unconditional_branch(self) -> bool:
        return self.op in {
            opcodepyro.opcode.JUMP_ABSOLUTE,
            opcodepyro.opcode.JUMP_FORWARD,
        }

    def is_relative_branch(self) -> bool:
        return self.op in {opcodepyro.opcode.FOR_ITER, opcodepyro.opcode.JUMP_FORWARD}

    def is_return(self) -> bool:
        return self.op == opcodepyro.opcode.RETURN_VALUE

    def is_raise(self) -> bool:
        return self.op == opcodepyro.opcode.RAISE_VARARGS

    def is_other_block_terminator(self):
        return self.op in {
            opcodepyro.opcode.RETURN_VALUE,
            opcodepyro.opcode.RAISE_VARARGS,
            # opcodepyro.opcode.RERAISE,
        }

    def is_terminator(self) -> bool:
        return self.is_branch() or self.is_other_block_terminator()

    def next_instr_offset(self) -> int:
        return self.next_instr_idx() * CODEUNIT_SIZE

    def next_instr_idx(self) -> int:
        return self.idx + 1

    def jump_target_offset(self) -> int:
        return self.jump_target_idx() * CODEUNIT_SIZE

    def jump_target_idx(self) -> int:
        if self.is_relative_branch():
            return self.next_instr_idx() + self.arg // CODEUNIT_SIZE
        return self.arg // CODEUNIT_SIZE

    def successor_indices(self) -> "Tuple[int]":
        if self.is_branch():
            if self.is_unconditional_branch():
                return (self.jump_target_idx(),)
            return (self.next_instr_idx(), self.jump_target_idx())
        if self.is_other_block_terminator():
            # Return, raise, etc have no successors
            return ()
        # Other instructions have implicit fallthrough to the next block
        return (self.next_instr_idx(),)

    def opname(self) -> str:
        return opcodepyro.opcode.opname[self.op]

    def __repr__(self) -> str:
        return f"{self.opname()} {self.arg}"


class BytecodeSlice:
    """A slice of bytecode from [start, end)."""

    def __init__(
        self,
        bytecode: "List[BytecodeOp]",
        start: "Optional[int]" = None,
        end: "Optional[int]" = None,
    ) -> None:
        self.bytecode = bytecode
        self.start: int = 0 if start is None else start
        self.end: int = len(bytecode) if end is None else end

    def last(self) -> BytecodeOp:
        return self.bytecode[self.end - 1]

    def successor_indices(self) -> "Tuple[int]":
        return self.last().successor_indices()

    def size(self) -> int:
        return self.end - self.start

    def __iter__(self) -> "Iterator[BytecodeOp]":
        return iter(self.bytecode[self.start : self.end])

    def __repr__(self) -> str:
        return f"<BytecodeSlice start={self.start}, end={self.end}>"


class BytecodeBlock:
    def __init__(self, id: int, bytecode: BytecodeSlice):
        self.id: int = id
        self.bytecode: BytecodeSlice = bytecode
        self.succs: "Tuple[BytecodeBlock]" = ()
        self.preds: Set[BytecodeBlock] = set()

    def name(self) -> str:
        return f"bb{self.id}"

    def instrs(self):
        return self.bytecode

    def __repr__(self) -> str:
        result = [f"{self.name()}:"]
        for instr in self.bytecode:
            result.append(f"  {instr}")
        return "\n".join(result)


class BlockMap:
    def __init__(self) -> None:
        self.idx_to_block: Dict[int, BytecodeBlock] = {}

    def add_block(self, idx, block):
        self.idx_to_block[idx] = block

    def entry(self):
        return self.idx_to_block[0]

    def __len__(self):
        return len(self.idx_to_block)

    def __iter__(self):
        return iter(self.idx_to_block.values())

    def __repr__(self) -> str:
        result = []
        for block in self.idx_to_block.values():
            result.append(f"bb{block.id}:")
            for instr in block.bytecode:
                if instr.is_branch():
                    succs = ", ".join(b.name() for b in block.succs)
                    result.append(f"  {instr.opname()} {succs}")
                else:
                    result.append(f"  {instr}")
        return "\n".join(result)


def code_to_ops(code: "CodeType") -> "List[BytecodeOp]":
    # TODO(emacs): Handle EXTENDED_ARG
    bytecode = code.co_code
    num_instrs = len(bytecode) // CODEUNIT_SIZE
    result: "List[BytecodeOp]" = [None] * num_instrs
    i = 0
    idx = 0
    while i < len(bytecode):
        op = bytecode[i]
        arg = bytecode[i + 1]
        result[idx] = BytecodeOp(op, arg, idx)
        i += CODEUNIT_SIZE
        idx += 1
    return result


def create_blocks(instrs: BytecodeSlice) -> BlockMap:
    """Construct a map of instructions to basic blocks, and a map of blocks to
    instruction ranges."""
    block_starts = set([0])
    num_instrs = instrs.size()
    # Mark the beginning of each basic block in the bytecode
    for instr in instrs:
        if instr.is_branch():
            block_starts.add(instr.next_instr_idx())
            block_starts.add(instr.jump_target_idx())
        elif instr.is_other_block_terminator():
            succ_idx = instr.next_instr_idx()
            if succ_idx < num_instrs:
                block_starts.add(succ_idx)
    # Allocate basic blocks corresponding to bytecode slices
    num_blocks = len(block_starts)
    block_starts_ordered = sorted(block_starts)
    block_map = BlockMap()
    for i, start_idx in enumerate(block_starts_ordered):
        # We may be making the last block, in which case there is not another
        # start index in the list. Use the number of bytecode instructions as
        # the end index.
        end_idx = block_starts_ordered[i + 1] if i + 1 < num_blocks else num_instrs
        block_instrs = BytecodeSlice(instrs.bytecode, start_idx, end_idx)
        block = BytecodeBlock(i, block_instrs)
        block_map.add_block(start_idx, block)
    for block in block_map.idx_to_block.values():
        succs = tuple(
            block_map.idx_to_block[idx]
            for idx in block.bytecode.successor_indices()
            if idx < num_instrs
        )
        for succ in succs:
            succ.preds.add(block)
        block.succs = succs
    return block_map


def optimize_load_fast(code):
    opcode = opcodepyro.opcode
    ops = code_to_ops(code)
    i = 0
    while i < len(code.co_code):
        if code.co_code[i] == opcode.EXTENDED_ARG:
            # Bail out; don't want to reshuffle EXTENDED_ARG
            return code
        i += CODEUNIT_SIZE

    blocks = create_blocks(BytecodeSlice(ops))
    num_blocks = len(blocks)
    preds = tuple(set() for i in range(num_blocks))
    for block in blocks:
        for succ in block.succs:
            preds[succ.id].add(block.id)

    num_locals = len(code.co_varnames)
    AllAssigned = 2**num_locals - 1
    entry = blocks.entry()
    # map of block id -> assignment state in lattice
    live_out = [AllAssigned] * num_blocks
    conditionally_assigned = set()
    argcount = (
        code.co_argcount
        + code.co_kwonlyargcount
        + bool(code.co_flags & CO_VARARGS)
        + bool(code.co_flags & CO_VARKEYWORDS)
    )
    ArgsAssigned = 2**argcount - 1

    def meet(args):
        result = AllAssigned
        for arg in args:
            result &= arg
        return result

    def process_one_block(block, modify=False):
        bid = block.id
        if len(preds[bid]) == 0:
            currently_alive = ArgsAssigned
        else:
            currently_alive = meet(live_out[pred] for pred in preds[bid])
        for instr in block.instrs():
            if modify and instr.op == opcode.LOAD_FAST:
                if currently_alive & (1 << instr.arg):
                    instr.op = opcode.LOAD_FAST_UNCHECKED
                elif instr.arg >= argcount:
                    # Exclude arguments because they come into the function
                    # body live. Anything that makes them no longer live will
                    # have to be DELETE_FAST at the beginning of the function.
                    conditionally_assigned.add(instr.arg)
            elif instr.op == opcode.STORE_FAST:
                currently_alive |= 1 << instr.arg
            elif instr.op == opcode.DELETE_FAST:
                currently_alive &= ~(1 << instr.arg)
        if currently_alive == live_out[block.id]:
            return False
        live_out[block.id] = currently_alive
        return True

    changed = True
    while changed:
        changed = False
        for block in blocks:
            changed |= process_one_block(block)

    for block in blocks:
        process_one_block(block, modify=True)

    optimized_bytecode = bytearray()
    if conditionally_assigned:
        for name_idx in sorted(conditionally_assigned):
            optimized_bytecode.append(opcode.DELETE_FAST_UNCHECKED)
            optimized_bytecode.append(name_idx)
    for instr in ops:
        assert instr.op != opcode.EXTENDED_ARG
        optimized_bytecode.append(instr.op)
        if conditionally_assigned and instr.op in opcode.hasjabs:
            instr.arg += len(conditionally_assigned) * CODEUNIT_SIZE
        if instr.arg >= 256:
            # Bail out; don't want to reshuffle EXTENDED_ARG
            return code
        optimized_bytecode.append(instr.arg)
    return code.replace(co_code=bytes(optimized_bytecode))


class PyroFlowGraph(PyFlowGraph38):
    opcode = opcodepyro.opcode

    def getCode(self):
        result = super().getCode()
        result = optimize_load_fast(result)
        return result


class ComprehensionRenamer(ASTVisitor):
    def __init__(self, scope):
        super().__init__()
        # We need a prefix that is unique per-scope for each renaming round.
        index = getattr(scope, "last_comprehension_rename_index", -1) + 1
        scope.last_comprehension_rename_index = index
        self.prefix = f"_gen{str(index) if index > 0 else ''}$"
        self.new_names = {}
        self.is_target = False

    def visitName(self, node):
        if self.is_target and isinstance(node.ctx, (ast.Store, ast.Del)):
            name = node.id
            new_name = self.prefix + name
            self.new_names[name] = new_name
            node.id = new_name
        else:
            new_name = self.new_names.get(node.id)
            if new_name is not None:
                node.id = new_name

    def visitarg(self, node):
        new_name = self.new_names.get(node.arg)
        if new_name is not None:
            node.arg = new_name


class CollectNames(ASTVisitor):
    def __init__(self):
        super().__init__()
        self.names = set()

    def visitName(self, node):
        self.names.add(node.id)

    def visitarg(self, node):
        self.names.add(node.arg)


def _can_inline_comprehension(node):
    can_inline = getattr(node, "can_inline", None)
    # Bad heuristic: Stop inlining comprehensions when "locals" is used.
    if can_inline is None:
        # Do not rename if "locals" is used.
        visitor = CollectNames()
        visitor.visit(node)
        can_inline = "locals" not in visitor.names
        node.can_inline = can_inline
    return can_inline


class PyroSymbolVisitor(SymbolVisitor):
    def visitDictCompListCompSetComp(self, node, scope):
        if not _can_inline_comprehension(node):
            return super().visitGeneratorExp(node, scope)

        # Check for unexpected assignments.
        scope.comp_iter_expr += 1
        self.visit(node.generators[0].iter, scope)
        scope.comp_iter_expr -= 1

        renamer = ComprehensionRenamer(scope)
        is_outer = True
        for gen in node.generators:
            renamer.visit(gen.iter)
            renamer.is_target = True
            renamer.visit(gen.target)
            renamer.is_target = False
            for if_node in gen.ifs:
                renamer.visit(if_node)

            self.visitcomprehension(gen, scope, is_outer)
            is_outer = False

        if isinstance(node, ast.DictComp):
            renamer.visit(node.value)
            renamer.visit(node.key)
            self.visit(node.value, scope)
            self.visit(node.key, scope)
        else:
            renamer.visit(node.elt)
            self.visit(node.elt, scope)

    visitDictComp = visitDictCompListCompSetComp
    visitListComp = visitDictCompListCompSetComp
    visitSetComp = visitDictCompListCompSetComp


class PyroCodeGenerator(Python38CodeGenerator):
    flow_graph = PyroFlowGraph

    @classmethod
    def make_code_gen(
        cls,
        name: str,
        tree: AST,
        filename: str,
        flags: int,
        optimize: int,
        peephole_enabled: bool = True,
        ast_optimizer_enabled: bool = True,
    ):
        if ast_optimizer_enabled:
            tree = cls.optimize_tree(optimize, tree)
        s = PyroSymbolVisitor()
        walk(tree, s)

        graph = cls.flow_graph(
            name, filename, s.scopes[tree], peephole_enabled=peephole_enabled
        )
        code_gen = cls(None, tree, s, graph, flags, optimize)
        walk(tree, code_gen)
        return code_gen

    @classmethod
    def optimize_tree(cls, optimize: int, tree: ast.AST):
        return AstOptimizerPyro(optimize=optimize > 0).visit(tree)

    def defaultEmitCompare(self, op):
        if isinstance(op, ast.Is):
            self.emit("COMPARE_IS")
        elif isinstance(op, ast.IsNot):
            self.emit("COMPARE_IS_NOT")
        else:
            self.emit("COMPARE_OP", self._cmp_opcode[type(op)])

    def visitListComp(self, node):
        if not _can_inline_comprehension(node):
            return super().visitListComp(node)
        self.emit("BUILD_LIST")
        self.compile_comprehension_body(node.generators, 0, node.elt, None, type(node))

    def visitSetComp(self, node):
        if not _can_inline_comprehension(node):
            return super().visitSetComp(node)
        self.emit("BUILD_SET")
        self.compile_comprehension_body(node.generators, 0, node.elt, None, type(node))

    def visitDictComp(self, node):
        if not _can_inline_comprehension(node):
            return super().visitDictComp(node)
        self.emit("BUILD_MAP")
        self.compile_comprehension_body(
            node.generators, 0, node.key, node.value, type(node)
        )


def compile(source, filename, mode, flags, dont_inherit, optimize):
    return compiler_compile(
        source, filename, mode, flags, None, optimize, PyroCodeGenerator
    )
