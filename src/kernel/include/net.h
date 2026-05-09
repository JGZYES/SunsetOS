#pragma once

#include <stdint.h>

namespace net {

void init();
int curl(const char* url);
int wget(const char* url, const char* filename);

}
