import argparse

DEFAULT_NUM_ITERATIONS = 2000000


class C:
    def __init__(self):
        self.num = 5

    @property
    def value(self):
        return self.num


def bench_property(num_iterations):
    c = C()
    for i in range(num_iterations):
        c.value


def run():
    bench_property(DEFAULT_NUM_ITERATIONS)


def warmup():
    bench_property(1)


def jit():
    try:
        from _builtins import _jit_fromlist

        _jit_fromlist(
            [
                bench_property,
            ]
        )
    except ImportError:
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "num_iterations",
        type=int,
        default=DEFAULT_NUM_ITERATIONS,
        nargs="?",
        help="Number of iterations to run the benchmark",
    )
    parser.add_argument("--jit", action="store_true", help="Run in JIT mode")
    args = parser.parse_args()
    warmup()
    if args.jit:
        jit()

    bench_property(args.num_iterations)
