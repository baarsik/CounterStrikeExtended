#include "dedicated/console/inherited_handle.h"
#include <cstdio>
#include <limits>
#include <string>
int main() {
    std::uintptr_t value = 0;
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (!cse::dedicated::ParseInheritedHandle(std::to_string(maximum - 1).c_str(), value)
        || value != maximum - 1) {
        return 1;
    }
    for (const char* invalid : {"", "-1", "+2", "1x", "0", " 12", "18446744073709551616"}) {
        if (cse::dedicated::ParseInheritedHandle(invalid, value)) {
            std::fprintf(stderr, "accepted invalid handle %s\n", invalid);
            return 2;
        }
    }
    if (cse::dedicated::ParseInheritedHandle(std::to_string(maximum).c_str(), value)) {
        return 3;
    }
    return 0;
}
