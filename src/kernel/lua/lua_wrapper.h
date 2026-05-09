#pragma once

#include <stddef.h>

namespace lua {

void init();
int run_script(const char* script);
int run_file(const char* filename);

}
