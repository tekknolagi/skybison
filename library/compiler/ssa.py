from . import opcode38
from .consts import CO_NEWLOCALS, CO_OPTIMIZED, CO_VARARGS, CO_VARKEYWORDS


class SSACFG:
    def __init__(self):
        self.blocks = {}

    def block_at(self, block):
        result = self.blocks.get(block)
        if result:
            return result
        return self.add_block(block)

    def add_block(self, block):
        assert block not in self.blocks
        result = SSABlock()
        self.blocks[block] = result
        return result

    def __repr__(self):
        result = ""
        for block, ssa_block in sorted(self.blocks.items(), key=lambda bs: bs[0].bid):
            result += f"{ssa_block.name()}:\n"
            for instr in ssa_block.instrs:
                result += f"  {instr}\n"
            result += "\n"
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

    def __init__(self):
        self.id = SSABlock.counter
        SSABlock.counter += 1
        self.instrs = []

    def emit(self, instr):
        self.instrs.append(instr)

    def name(self):
        return f"bb{self.id}"


OPCODE_INFO = {
    # (input, output)
    "LOAD_FAST": (0, 1),
    "LOAD_FAST_UNCHECKED": (0, 1),
    "BINARY_MULTIPLY": (2, 1),
    "BINARY_ADD": (2, 1),
    "STORE_FAST": (1, 0),
    "LOAD_CONST": (0, 1),
    "DELETE_FAST": (0, 0),
    "RETURN_VALUE": (1, 0),
    "POP_JUMP_IF_FALSE": (1, 0),
}


class SSA:
    def __init__(self, graph):
        self.graph = graph

    def run(self):
        blocks = self.graph.getBlocksInOrder()
        preds = {block: set() for block in blocks}
        succs = {}
        for block in blocks:
            children = [child for child in block.get_children() if child]
            succs[block] = children
            for child in children:
                preds[child].add(block)

        current_def = {}

        def write_variable(variable, block, value):
            if variable not in current_def:
                current_def[variable] = {}
            current_def[variable][block] = value

        def delete_variable(variable, block):
            if variable not in current_def:
                current_def[variable] = {}
            del current_def[variable][block]

        def read_variable_recursive(variable, block):
            assert len(preds[block]) != 0
            if len(preds[block]) == 1:
                (pred,) = preds[block]
                return read_variable(variable, pred)
            phi = SSAInstruction("Phi", [], None)
            cfg.block_at(block).emit(phi)
            write_variable(variable, block, phi)
            for pred in preds[block]:
                defn = read_variable(variable, pred)
                phi.args.append(defn)
            return phi

        def read_variable(variable, block):
            if variable not in current_def:
                ssa_instr = SSAInstruction("Undef", (), None)
                write_variable(variable, entry, ssa_instr)
                entry.emit(ssa_instr)
            result = current_def[variable].get(block)
            if result:
                return result
            return read_variable_recursive(variable, block)

        def copy_instr(instr, operands):
            return SSAInstruction(instr.opname, operands, instr)

        cfg = SSACFG()

        entry = blocks[0]
        assert len(preds[entry]) == 0
        argcount = (
            len(self.graph.args)
            + len(self.graph.kwonlyargs)
            + (self.graph.flags & CO_VARARGS)
            + (self.graph.flags & CO_VARKEYWORDS)
        )
        argnames = (*self.graph.varnames,)[:argcount]
        ssa_entry = cfg.block_at(entry)
        for idx, argname in enumerate(argnames):
            ssa_instr = SSAInstruction(f"LoadArg<{idx}; {argname}>", (), None)
            ssa_entry.emit(ssa_instr)
            write_variable(argname, entry, ssa_instr)

        for block in blocks:
            ssa_block = cfg.block_at(block)
            instrs = block.getInstructions()
            stack = []
            for instr in instrs:
                if instr.opname == "SET_LINENO":
                    continue
                num_operands, num_outputs = OPCODE_INFO[instr.opname]
                assert num_outputs <= 1
                assert len(stack) >= num_operands
                operands = stack[-num_operands:]
                for i in range(num_operands):
                    stack.pop()
                opcode = opcode38.opcode.opmap[instr.opname]
                if instr.opname == "STORE_FAST":
                    write_variable(instr.oparg, block, operands[0])
                elif instr.opname == "DELETE_FAST":
                    delete_variable(instr.oparg, block)
                elif instr.opname in ("LOAD_FAST", "LOAD_FAST_UNCHECKED"):
                    ssa_instr = read_variable(instr.oparg, block)
                    stack.append(ssa_instr)
                elif instr.opname == "LOAD_CONST":
                    ssa_instr = SSAInstruction(f"LOAD_CONST<{instr.oparg}>", (), instr)
                    stack.append(ssa_instr)
                    ssa_block.emit(ssa_instr)
                elif (
                    opcode in opcode38.opcode.hasjrel
                    or opcode in opcode38.opcode.hasjabs
                ):
                    # TODO(max): Sort depending on opcode (e.g.
                    # POP_JUMP_IF_FALSE, POP_JUMP_IF_TRUE)
                    targets = tuple(cfg.block_at(target) for target in succs[block])
                    ssa_instr = SSAInstruction(
                        instr.opname, (operands[0],), instr, targets
                    )
                    ssa_block.emit(ssa_instr)
                else:
                    ssa_instr = copy_instr(instr, operands)
                    ssa_block.emit(ssa_instr)
                    if num_outputs:
                        stack.append(ssa_instr)
                if instr.opname in ("RETURN_VALUE", "RAISE_VARARGS"):
                    # CPython blocks keep dead code at the end. Terminate
                    # early.
                    break
            term = instrs[-1].opname
            if "JUMP" not in term and term not in ("RETURN_VALUE", "RAISE_VARARGS"):
                (succ,) = succs[block]
                target = cfg.block_at(succ)
                ssa_block.emit(SSAInstruction("Branch", (), None, (target,)))
        print(cfg)
