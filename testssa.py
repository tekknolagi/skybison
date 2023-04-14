def undef():
    "$SSA$"
    return w
    w = 1


def phi(cond, x, y):
    "$SSA$"
    if cond:
        x = y * x
    z = x + 1
    return z


def loopy(xs):
    "$SSA$"
    result = []
    for item in xs:
        result.append(item)
    return result
