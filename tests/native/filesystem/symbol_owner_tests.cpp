#include "public/utlsymbol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

}

int main() {
    CUtlSymbolTable independent;
    const CUtlSymbol independentName = independent.AddString("independent table");
    CUtlSymbol::AcquireFileSystemOwner();
    CUtlSymbol first("first filesystem");
    CUtlSymbol::AcquireFileSystemOwner();
    CUtlSymbol second("second filesystem");
    Require(!CUtlSymbol::ReleaseFileSystemOwner(), "first detach retains a table shared by another filesystem");
    Require(std::strcmp(first.String(), "first filesystem") == 0, "first symbol remains readable for peer owner");
    Require(std::strcmp(second.String(), "second filesystem") == 0, "peer symbol remains readable");
    Require(CUtlSymbol::ReleaseFileSystemOwner(), "final owner detaches shared table storage");
    Require(std::strcmp(independent.String(independentName), "independent table") == 0,
        "filesystem detach leaves independently owned symbol tables alive");
    CUtlSymbol::AcquireFileSystemOwner();
    CUtlSymbol restarted("restarted filesystem");
    Require(std::strcmp(restarted.String(), "restarted filesystem") == 0, "fresh owner can initialize a new table");
    Require(CUtlSymbol::ReleaseFileSystemOwner(), "restarted owner can detach cleanly");
    return EXIT_SUCCESS;
}
