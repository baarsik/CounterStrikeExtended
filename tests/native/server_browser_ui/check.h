#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>

inline void Require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <class Function> int RunCase(Function function) {
    try {
        function();
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
