#include <Windows.h>
#include "host.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace {
constexpr SIZE_T MinimumBytes = 14u * 1024u * 1024u;
constexpr SIZE_T MaximumBytes = 128u * 1024u * 1024u;
void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL actual Windows failure reference: %s\n", message);
        std::abort();
    }
}
struct FatalCall {
    std::string format;
    int bytes = 0;
    double mebibytes = 0;
};
}

#define CSE_FAILURE_OWNER sw
#define CSE_FAILURE_SW 1
#include "windows_failure_owner.inc"
#undef CSE_FAILURE_SW
#undef CSE_FAILURE_OWNER
#define CSE_FAILURE_OWNER hw
#define CSE_FAILURE_SW 0
#include "windows_failure_owner.inc"
#undef CSE_FAILURE_SW
#undef CSE_FAILURE_OWNER

namespace {
template<typename Owner>
void Run(const char* name) {
    int rows = 0;
    for (const unsigned long long physical : {1ull, 14680064ull, 15728639ull}) {
        for (const bool dedicated : {false, true}) {
            for (const bool minimum : {false, true}) {
                for (const bool cli : {false, true}) {
                    std::vector<std::string> arguments{"engine"};
                    if (cli) {
                        arguments.push_back("-heapsize");
                        arguments.push_back("1");
                    }
                    if (minimum) {
                        arguments.push_back("-minmemory");
                    }
                    Owner::Set(0, physical, dedicated, arguments);
                    bool caught = false;
                    try {
                        Owner::Init();
                    } catch (const FatalCall& call) {
                        caught = true;
                        Check(call.format == "Available memory less than 15MB!!! %i\n"
                            && call.bytes == (cli ? 1024 : 0), "original diagnostic before all overrides");
                    }
                    Check(caught && Owner::Trace() == "HQE" && Owner::Allocations() == 0,
                        "known low RAM fatal before dedicated/minimum/allocation");
                    Owner::Shutdown();
                    Owner::Shutdown();
                    Check(Owner::Heap().membase == nullptr && Owner::Heap().memsize == 0, "partial-init cleanup");
                    ++rows;
                }
            }
        }
    }
    Owner::Set(67108864, 68719476736ull, false, {"engine"});
    bool caught = false;
    try {
        Owner::Init();
    } catch (const FatalCall& call) {
        caught = true;
        Check(call.format == "Unable to allocate %.2f MB\n" && call.mebibytes == 64.0,
            "original allocation-failure diagnostic");
    }
    Check(caught && Owner::Trace() == "HMAE" && Owner::Allocations() == 1,
        "single explicit heap attempt then fatal, no query");
    Check(Owner::Heap().membase == nullptr && Owner::Heap().memsize == 67108864, "failed allocation host state");
    Owner::Shutdown();
    Owner::Shutdown();
    Check(Owner::Heap().membase == nullptr && Owner::Heap().memsize == 0, "failed allocation repeated cleanup");
    std::printf("PASS original Windows %s: %d known-low-RAM order cases + allocation failure\n", name, rows);
}
}

int main() {
    Run<sw::Adapter>("SW");
    Run<hw::Adapter>("HW");
}
