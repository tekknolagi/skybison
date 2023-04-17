from . import opcode38
from .consts import CO_NEWLOCALS, CO_OPTIMIZED, CO_VARARGS, CO_VARKEYWORDS


class SSACFG:
    def __init__(self):
        self.blocks = {}

    def block_at(self, start_instr):
        result = self.blocks.get(start_instr)
        if result:
            return result
        return self.add_block(start_instr)

    def add_block(self, start_instr):
        assert start_instr not in self.blocks
        result = SSABlock()
        self.blocks[start_instr] = result
        return result

    def __repr__(self):
        result = ""
        for _, ssa_block in self.blocks.items():
            result += f"{ssa_block.name()}:\n"
            for instr in ssa_block.instrs:
                result += f"  {instr}\n"
            result += f"(succs {ssa_block.succs})\n"
        return result


class SSAInstruction:
    counter = 0
    __slots__ = ("id", "opname", "args", "instr", "targets")

    def __init__(self, opname, args, instr, targets=()):
        self.id = SSAInstruction.counter
        SSAInstruction.counter += 1
        self.opname = opname
        self.args = args
        self.instr = instr
        self.targets = targets

    def output(self):
        return f"v{self.id}"

    def __repr__(self):
        if self.targets:
            immediates = "<" + ", ".join(target.name() for target in self.targets) + ">"
        else:
            immediates = ""
        return f"{self.output()} = {self.opname}{immediates} {' '.join(arg.output() for arg in self.args)}"


class SSABlock:
    counter = 0
    __slots__ = ("id", "instrs", "succs")

    def __init__(self):
        self.id = SSABlock.counter
        SSABlock.counter += 1
        self.instrs = []
        self.succs = ()

    def emit(self, instr):
        self.instrs.append(instr)

    def name(self):
        return f"bb{self.id}"

    def __repr__(self):
        return self.name()


class Interpreter:
    def __init__(self, cfg, entry, preds):
        self.cfg = cfg
        self.stack = []
        self.block = None
        self.ssa_block = None
        self.entry = entry
        self.preds = preds
        self.current_def = {}

    def emit(self, ssa_instr):
        self.ssa_block.emit(ssa_instr)

    def clone(self, instr, operands):
        ssa_instr = SSAInstruction(instr.opname, operands, instr)
        self.emit(ssa_instr)
        return ssa_instr

    def write_variable(self, variable, block, value):
        if variable not in self.current_def:
            self.current_def[variable] = {}
        self.current_def[variable][block] = value

    def delete_variable(self, variable, block):
        if variable not in self.current_def:
            self.current_def[variable] = {}
        del self.current_def[variable][block]

    def read_variable_recursive(self, variable, block):
        assert len(self.preds[block]) != 0, f"cannot find {block}"
        if len(self.preds[block]) == 1:
            (pred,) = self.preds[block]
            return self.read_variable(variable, pred)
        phi = SSAInstruction("Phi", [], None)
        self.emit(phi)
        self.write_variable(variable, block, phi)
        for pred in self.preds[block]:
            defn = self.read_variable(variable, pred)
            phi.args.append(defn)
        return phi

    def read_variable(self, variable, block):
        if variable not in self.current_def:
            ssa_instr = SSAInstruction("Undef", (), None)
            self.write_variable(variable, self.entry, ssa_instr)
            self.cfg.block_at(self.entry.getInstructions()[0]).emit(ssa_instr)
        result = self.current_def[variable].get(block)
        if result:
            return result
        return self.read_variable_recursive(variable, block)

    def do_SET_LINENO(self, instr):
        pass

    def do_LOAD_FAST(self, instr):
        result = self.read_variable(instr.oparg, self.block)
        self.stack.append(result)

    do_LOAD_FAST_UNCHECKED = do_LOAD_FAST

    def do_LOAD_CONST(self, instr):
        value = SSAInstruction(f"LOAD_CONST<{instr.oparg}>", (), instr)
        self.stack.append(value)
        self.emit(value)

    def do_BINARY_MULTIPLY(self, instr):
        rhs = self.stack.pop()
        lhs = self.stack.pop()
        result = self.clone(instr, (lhs, rhs))
        self.stack.append(result)

    do_BINARY_ADD = do_BINARY_MULTIPLY

    def do_STORE_FAST(self, instr):
        value = self.stack.pop()
        self.write_variable(instr.oparg, self.block, value)

    def do_DELETE_FAST(self, instr):
        self.delete_variable(instr.oparg, self.block)

    def targets(self):
        return tuple(self.cfg.block_at(target) for target in self.succs[self.block])

    def do_any_jump(self, instr):
        # TODO(max): Sort targets depending on opcode (e.g. POP_JUMP_IF_FALSE,
        # POP_JUMP_IF_TRUE)
        if "IF" in instr.opname:
            cond = self.stack.pop()
            ssa_instr = self.clone(instr, (cond,))
        else:
            ssa_instr = self.clone(instr, ())
        ssa_instr.targets = self.targets()

    do_POP_JUMP_IF_FALSE = do_any_jump

    do_JUMP_ABSOLUTE = do_any_jump

    def do_RETURN_VALUE(self, instr):
        result = self.stack.pop()
        self.clone(instr, (result,))

    def do_BUILD_LIST(self, instr):
        operands = reversed([self.stack.pop() for i in range(instr.oparg)])
        result = self.clone(instr, (*operands,))
        self.stack.append(result)

    def do_GET_ITER(self, instr):
        iterable = self.stack.pop()
        result = self.clone(instr, (iterable,))
        self.stack.append(result)

    def do_FOR_ITER(self, instr):
        # TODO(max): Split the block. This will require multiple SSA blocks per
        # block, so we can't use block_at anymore.
        iterator = self.stack[-1]
        result = self.clone(instr, (iterator,))
        self.stack.append(result)
        self.emit(
            SSAInstruction(
                "CondBranchIterNotDone", (result,), instr, targets=self.targets()
            )
        )

    def do_LOAD_METHOD(self, instr):
        receiver = self.stack.pop()
        result = self.clone(instr, (receiver,))
        self.stack.append(result)
        method = SSAInstruction("GetSecondOutput", (result,), instr)
        self.emit(method)
        self.stack.append(method)

    def do_CALL_METHOD(self, instr):
        # one for receiver and one for method
        operands = reversed([self.stack.pop() for i in range(instr.oparg + 2)])
        result = self.clone(instr, (*operands,))
        self.stack.append(result)

    def do_POP_TOP(self, instr):
        self.stack.pop()


def is_cond_branch(instr):
    return instr.opname in frozenset(
        {
            "FOR_ITER",
            "POP_JUMP_IF_FALSE",
            "JUMP_IF_FALSE_OR_POP",
            "JUMP_IF_TRUE_OR_POP",
        }
    )


def is_branch(instr):
    return instr.opname in frozenset(
        {
            "FOR_ITER",
            "JUMP_ABSOLUTE",
            "JUMP_FORWARD",
            "JUMP_IF_FALSE_OR_POP",
            "JUMP_IF_TRUE_OR_POP",
            "JUMP_IF_NOT_EXC_MATCH",
            "POP_JUMP_IF_FALSE",
            "POP_JUMP_IF_TRUE",
        }
    )


def is_other_block_terminator(instr):
    return instr.opname in frozenset({"RETURN_VALUE", "RAISE_VARARGS", "RERAISE"})


def is_terminator(instr):
    return is_branch(instr) or is_other_block_terminator(instr)


class SSA:
    def __init__(self, graph):
        self.graph = graph

    def run(self):
        blocks = self.graph.getBlocksInOrder()
        # Build SSA blocks at each terminator
        cfg = SSACFG()
        entry = blocks[0]
        current_block = cfg.block_at(entry.getInstructions()[0])
        last_instr = None
        for block in blocks:
            for instr in block.getInstructions():
                if last_instr and is_terminator(last_instr):
                    previous_block = current_block
                    current_block = cfg.block_at(instr)
                    # if is_cond_branch(last_instr):
                    #     previous_block.succs = (current_block,
                    #             last_instr.target.getInstructions()[0])
                    # else:
                    #     previous_block.succs = (current_block,)
                last_instr = instr
        for ssa_block in cfg.blocks.values():
            pass
        print(cfg)

        # # Find predecessors and successors
        # preds = {block: set() for block in cfg.blocks.values()}
        # succs = {}
        # for block in cfg.blocks.values():
        #     children = frozenset(child for child in block.get_children() if child)
        #     succs[block] = children
        #     for child in children:
        #         preds[child].add(block)

        # assert len(preds[entry]) == 0
        # argcount = (
        #     len(self.graph.args)
        #     + len(self.graph.kwonlyargs)
        #     + (self.graph.flags & CO_VARARGS)
        #     + (self.graph.flags & CO_VARKEYWORDS)
        # )
        # argnames = (*self.graph.varnames,)[:argcount]
        # ssa_entry = cfg.block_at(entry)
        # interpreter = Interpreter(cfg, entry, succs, preds)
        # for idx, argname in enumerate(argnames):
        #     ssa_instr = SSAInstruction(f"LoadArg<{idx}; {argname}>", (), None)
        #     ssa_entry.emit(ssa_instr)
        #     interpreter.write_variable(argname, entry, ssa_instr)

        # interpreter.block = entry
        # interpreter.ssa_block = cfg.block_at(entry)
        # for block in blocks:
        #     instrs = block.getInstructions()
        #     stack = []
        #     found_term = False
        #     for instr in instrs:
        #         if instr in cfg.blocks:
        #             interpreter.ssa_block = cfg.blocks[instr]
        #         getattr(interpreter, f"do_{instr.opname}")(instr)
        #         if instr.opname in ("RETURN_VALUE", "RAISE_VARARGS"):
        #             # CPython blocks keep dead code at the end. Terminate
        #             # early.
        #             found_term = True
        #             break
        #         if "JUMP" in instr.opname:
        #             # CPython blocks keep dead code at the end. Terminate
        #             # early.
        #             found_term = True
        #             break
        #         if instr.opname == "FOR_ITER":
        #             # CPython does not break up blocks after FOR_ITER
        #             print("iter target is", instr.target)
        #             interpreter.block = instr.target
        #             interpreter.ssa_block = cfg.block_at(instr.target)
        #     # TODO(max): Handle opcodes that only touch the stack on one branch
        #     # FOR_ITER, JUMP_IF_FALSE_OR_POP, JUMP_IF_TRUE_OR_POP, YIELD_FROM
        #     if not found_term:
        #         (succ,) = succs[block]
        #         target = cfg.block_at(succ)
        #         cfg.block_at(block).emit(SSAInstruction("Branch", (), None, (target,)))
        # print(cfg)
