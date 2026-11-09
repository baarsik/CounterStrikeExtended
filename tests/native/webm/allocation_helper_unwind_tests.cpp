#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>

short* InvokePcmFromNoEh(unsigned int count);
unsigned int helperInnerDestructors = 0;

namespace {

unsigned int outerDestructors = 0;
unsigned int allocationCalls = 0;

struct OuterLifetime {
    ~OuterLifetime() {
        ++outerDestructors;
    }
};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "PCM unwind fixture: %s\n", message);
        std::exit(1);
    }
}

}

void* operator new[](std::size_t) {
    ++allocationCalls;
    throw std::bad_alloc();
}

void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}

#if defined(__cpp_sized_deallocation)
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
#endif

int main() {
    bool caughtAllocationFailure = false;
    try {
        OuterLifetime lifetime;
        delete[] InvokePcmFromNoEh(4096);
    } catch (const std::bad_array_new_length&) {
    } catch (const std::bad_alloc&) {
        caughtAllocationFailure = true;
    }
    Require(caughtAllocationFailure && helperInnerDestructors == 0 && outerDestructors == 1
            && allocationCalls == 1,
        "real helper bad_alloc crosses no-EH caller to outer catch without inner destruction");
    bool caughtInvalidCount = false;
    try {
        OuterLifetime lifetime;
        delete[] InvokePcmFromNoEh(UINT_MAX);
    } catch (const std::bad_array_new_length&) {
        caughtInvalidCount = true;
    } catch (...) {
    }
    Require(caughtInvalidCount && helperInnerDestructors == 0 && outerDestructors == 2 && allocationCalls == 1,
        "real helper invalid count crosses no-EH caller to outer catch before allocator invocation");
    std::puts("real PCM helper unwind passed: bad_alloc + bad_array_new_length; no-EH inner destructors0/outer2");
    return 0;
}
