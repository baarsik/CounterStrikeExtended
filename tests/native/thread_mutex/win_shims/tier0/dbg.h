#pragma once

#include <cstdlib>

#define DBG_BUILDSLAVE_PATH(relative) relative
#define AssertMsgAtFile(expression, message, file, line) \
    do { \
        if (!(expression)) { \
            std::abort(); \
        } \
    } while (false)
