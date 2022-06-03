Implementation of `_valgrind` module for cpython. The `_valgrind` module allows
to control the valgrind/callgrind mode to enable/disable profiling from within
python code.

Setup:
    /path/to/my/python setup.py build

Then copy the results of build/lib.*/*.so somewhere that is part of your
sys.path.
