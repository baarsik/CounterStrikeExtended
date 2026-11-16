#include "backend_selection.h"
#include <iostream>
#include <cstdlib>
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << __LINE__ << ": " #condition << "\n"; \
        std::abort(); \
    } \
} while (false)

using namespace cse::render;
int main() {
    CHECK(SelectBackend(true, true, false, false).backend == Backend::Metal);
    CHECK(SelectBackend(true, true, true, false).backend == Backend::OpenGL);
    CHECK(SelectBackend(true, true, false, true).backend == Backend::Metal);
    CHECK(SelectBackend(true, true, true, true).error);
    CHECK(SelectBackend(true, false, false, false).backend == Backend::OpenGL);
    CHECK(SelectBackend(true, false, false, true).error);
    CHECK(SelectBackend(false, false, false, false).backend == Backend::OpenGL);
    CHECK(SelectBackend(false, true, false, true).error);
    CHECK(SelectBackend(true, true, false, false, true).backend == Backend::OpenGL);
    CHECK(SelectBackend(true, true, false, true, true).backend == Backend::Metal);
    CHECK(SelectBackend(true, true, true, false, false).backend == Backend::OpenGL);
    CHECK(SelectBackend(true, true, true, true, true).error);
    CHECK(SelectBackend(true, false, false, false, false).backend == Backend::OpenGL);
    CHECK(SelectBackend(false, true, false, false, false).backend == Backend::OpenGL);
    std::cout << "14 selection cases passed\n";
}
