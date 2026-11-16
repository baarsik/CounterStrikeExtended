#include "Test.h"

#include <cstdio>

int main() {
    int failures = 0;
    for (const auto& test : radar_test::Cases()) {
        try {
            test.run();
        } catch (const std::exception& exception) {
            std::fprintf(stderr, "FAIL %s: %s\n", test.name, exception.what());
            ++failures;
        }
    }
    std::printf("%zu radar cases, %d failures\n", radar_test::Cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
