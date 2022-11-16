#pragma once

#include "handles.h"
#include "thread.h"

namespace py {

RawObject ssaify(Thread* thread, const Function& function);

}
