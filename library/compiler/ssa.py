from .consts import CO_NEWLOCALS, CO_OPTIMIZED, CO_VARARGS, CO_VARKEYWORDS

class SSAInstruction:
    counter = 0
    __slots__ = ("id", "opname", "args", "instr")

    def __init__(self, opname, args, instr):
        self.id  = SSAInstruction.counter
        SSAInstruction.counter += 1
        self.opname = opname
        self.args = args
        self.instr = instr

    def __repr__(self):
        return f"v{self.id} = {self.opname} {' '.join(self.args)}"


class SSA:
    def __init__(self, graph):
        self.graph = graph

    def run(self):
        blocks = self.graph.getBlocksInOrder()
        preds = {block: set() for block in blocks}
        for block in blocks:
            for child in block.get_children():
                if not child:
                    continue
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
                pred, = preds[block]
                return read_variable(variable, pred)
            phi = Instruction("Phi", [], 0, None)
            write_variable(variable, block, phi)
            for pred in preds[block]:
                defn = read_variable(variable, pred)
                phi.oparg.append(defn)
            return phi

        def read_variable(variable, block):
            if variable not in current_def:
                write_variable(variable, entry, SSAInstruction("Undef", (), None))
            result = current_def[variable].get(block)
            if result:
                return result
            return read_variable_recursive(variable, block)

        def copy_instr(instr):
            return SSAInstruction(instr.opname, (str(instr.oparg),), instr)

        entry = blocks[0]
        assert len(preds[entry]) == 0
        argcount = len(self.graph.args) + len(self.graph.kwonlyargs) + \
                (self.graph.flags & CO_VARARGS) + \
                (self.graph.flags & CO_VARKEYWORDS)
        argnames = (*self.graph.varnames,)[:argcount]
        for argname in argnames:
            write_variable(argname, entry, SSAInstruction("LoadArg", (argname,), None))

        for block in blocks:
            instrs = block.getInstructions()
            ssa_instrs = []
            for instr in instrs:
                if instr.opname == "SET_LINENO":
                    continue
                ssa_instr = copy_instr(instr)
                ssa_instrs.append(ssa_instr)
                if instr.opname == "STORE_FAST":
                    write_variable(instr.oparg, block, ssa_instr)
                elif instr.opname in ("LOAD_FAST", "LOAD_FAST_UNCHECKED"):
                    ssa_instr = read_variable(instr.oparg, block)
                elif instr.opname == "DELETE_FAST":
                    delete_variable(instr.oparg, block)
                if instr.opname in ("RETURN_VALUE", "RAISE_VARARGS"):
                    # CPython blocks keep dead code at the end. Terminate
                    # early.
                    break
            #print(ssa_instrs)
        #print(current_def)
