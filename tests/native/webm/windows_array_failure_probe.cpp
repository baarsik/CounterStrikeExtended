#include <climits>
#include <cstdio>
#include <new>

int main() {
    static_assert(sizeof(void*) == 4, "Probe the retained Win32 default allocation failure path only");
    const unsigned int counts[] = {UINT_MAX / sizeof(short), UINT_MAX / sizeof(short) + 1, UINT_MAX};
    for (unsigned int value : counts) {
        volatile unsigned int count = value;
        try {
            short* allocation = new short[count];
            std::printf("%u,returned\n", value);
            delete[] allocation;
        } catch (const std::bad_array_new_length&) {
            std::printf("%u,bad-array-new-length\n", value);
        } catch (const std::bad_alloc&) {
            std::printf("%u,bad-alloc\n", value);
        } catch (...) {
            std::printf("%u,other\n", value);
        }
    }
    return 0;
}
