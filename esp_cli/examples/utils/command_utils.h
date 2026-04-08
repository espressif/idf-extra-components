
#pragma once

#include "unistd.h"

#define WRITE_FN(fn, fd, fmt, ...) do {                           \
    char _buf[256];                                               \
    int _len = snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__);  \
    if (_len > 0)                                                 \
        fn(fd, _buf, _len);                                       \
} while(0)
