#include "globals.h"

/* GDB puts a breakpoint in this function.
 *
 * Has to be on another file than the caller as otherwise gcc may
 * optimize away the call. */
PY_EXPORT NEVER_INLINE void __jit_debug_register_code(void);
PY_EXPORT NEVER_INLINE void __jit_debug_register_code(void) {}
