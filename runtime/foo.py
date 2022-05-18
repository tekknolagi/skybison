import symtable

code = """
def foo(x):
    if x:
        y = 1
    return y
"""

def rec_print(st, indent=""):
    print(indent, st, sep="", end=" / ")
    print(st.get_identifiers(), end=" / ")
    print(st.get_symbols())
    if isinstance(st, symtable.Function):
        print(indent+"  ", st.get_locals())
    for child in st.get_children():
        rec_print(child, indent+"  ")

st = symtable.symtable(code, "filename", "exec")
rec_print(st)
