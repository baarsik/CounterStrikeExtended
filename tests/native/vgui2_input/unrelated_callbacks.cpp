#include <cstdio>
#include <cstdlib>

// Kept out of the imported-method TU: a deliberate unexpected callback remains
// fatal without making the retained Windows fallback syntactically unreachable.
void UnexpectedInputCallback() {
    std::fputs("FAIL: unexpected cursor bounds/global fallback binding\n", stderr);
    std::abort();
}
