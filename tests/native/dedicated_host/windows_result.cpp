#include <cstdio>

int main() {
    constexpr int ProviderRestartState = 5;
    for (const int state : {DLL_INACTIVE, DLL_ACTIVE, DLL_PAUSED, DLL_CLOSE, DLL_TRANS, ProviderRestartState}) {
        if (ActualMainResult(state) != 0) {
            std::fprintf(stderr, "normal Windows terminal state %d became process failure\n", state);
            return 1;
        }
    }
    for (const int failure : {-1, -2}) {
        if (ActualMainResult(failure) != failure) {
            std::fprintf(stderr, "Windows startup failure %d was lost\n", failure);
            return 2;
        }
    }
    return 0;
}
