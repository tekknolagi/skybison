# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import ast
from ast import AST
from compiler import compile as compiler_compile
from compiler.consts import CO_VARARGS, CO_VARKEYWORDS
from compiler.optimizer import BIN_OPS, is_const, get_const_value
from compiler.py38.optimizer import AstOptimizer38
from compiler.pyassem import PyFlowGraph38, Instruction
from compiler.pycodegen import Python38CodeGenerator
from compiler.symbols import SymbolVisitor
from compiler.visitor import ASTVisitor, walk

import _compiler_opcode as opcodepyro


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


class PyroFlowGraph(PyFlowGraph38):
    opcode = opcodepyro.opcode

    def optimizeStoreFast(self):
        if "locals" in self.varnames or "locals" in self.names:
            # A bit of a hack: if someone is using locals(), we shouldn't mess
            # with them.
            return
        used = set()
        for block in self.getBlocksInOrder():
            for instr in block.getInstructions():
                if instr.opname == "LOAD_FAST" or instr.opname == "DELETE_FAST":
                    used.add(instr.oparg)
        # We never read from or delete the local, so we can replace all stores
        # to it with POP_TOP.
        for block in self.getBlocksInOrder():
            for instr in block.getInstructions():
                if instr.opname == "STORE_FAST" and instr.oparg not in used:
                    instr.opname = "POP_TOP"
                    instr.oparg = 0
                    instr.ioparg = 0

    def optimizeLoadFast(self):
        blocks = self.getBlocksInOrder()
        preds = tuple(set() for i in range(self.block_count))
        for block in blocks:
            for child in block.get_children():
                if child is not None:
                    # TODO(emacs): Tail-duplicate finally blocks or upgrade to
                    # 3.10, which does this already. This avoids except blocks
                    # falling through into else blocks and mucking up
                    # performance.
                    preds[child.bid].add(block.bid)

        num_locals = len(self.varnames)
        Top = 2**num_locals - 1
        # map of block id -> assignment state in lattice
        assigned_out = [Top] * self.block_count
        conditionally_assigned = set()
        argcount = (
            len(self.args)
            + len(self.kwonlyargs)
            + bool(self.flags & CO_VARARGS)
            + bool(self.flags & CO_VARKEYWORDS)
        )
        total_locals = num_locals + len(self.cellvars) + len(self.freevars)
        ArgsAssigned = 2**argcount - 1

        def reverse_local_idx(idx):
            return total_locals - idx - 1

        def meet(args):
            result = Top
            for arg in args:
                result &= arg
            return result

        def process_one_block(block, modify=False):
            bid = block.bid
            if len(preds[bid]) == 0:
                # No preds; all parameters are assigned
                assigned = ArgsAssigned
            else:
                # Meet the assigned sets of all predecessors
                assigned = meet(assigned_out[pred] for pred in preds[bid])
            for instr in block.getInstructions():
                if modify and instr.opname == "LOAD_FAST":
                    if assigned & (1 << instr.ioparg):
                        instr.opname = "LOAD_FAST_REVERSE_UNCHECKED"
                        instr.ioparg = reverse_local_idx(instr.ioparg)
                    elif instr.ioparg >= argcount:
                        # Exclude arguments because they come into the function
                        # body assigned. The only thing that can undefine them
                        # is DELETE_FAST.
                        conditionally_assigned.add(instr.oparg)
                elif instr.opname == "STORE_FAST":
                    assigned |= 1 << instr.ioparg
                    if modify:
                        instr.opname = "STORE_FAST_REVERSE"
                        instr.ioparg = reverse_local_idx(instr.ioparg)
                elif instr.opname == "DELETE_FAST":
                    assigned &= ~(1 << instr.ioparg)
            if assigned == assigned_out[bid]:
                return False
            assigned_out[bid] = assigned
            return True

        changed = True
        while changed:
            changed = False
            for block in blocks:
                changed |= process_one_block(block)

        for block in blocks:
            process_one_block(block, modify=True)

        if conditionally_assigned:
            deletes = [
                Instruction(
                    "DELETE_FAST_REVERSE_UNCHECKED",
                    name,
                    reverse_local_idx(self.varnames.index(name)),
                )
                for name in sorted(conditionally_assigned)
            ]
            self.entry.insts = deletes + self.entry.insts

    def getCode(self):
        self.optimizeStoreFast()
        self.optimizeLoadFast()
        return super().getCode()


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
