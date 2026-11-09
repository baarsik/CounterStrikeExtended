#include "legacy_str_to_integer.h"
#include "input_cases.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s errno=%d\n", message, errno);
        std::abort();
    }
}
}

int main(int argc, char** argv) {
    Require(argc == 3, "golden table and explicit byte policy required");
    const auto profile = std::string(argv[2]) == "nbsp" ? CseLegacyIntegerByteWhitespace::SingleByteNbsp
        : CseLegacyIntegerByteWhitespace::COrUtf8;
    const auto cases = InputCases();
    std::unordered_map<std::string, const std::string*> texts;
    for (const auto& input : cases) {
        texts.emplace(input.name, &input.text);
    }
    std::ifstream golden(argv[1]);
    Require(golden.is_open(), "frozen actual UCRT table");
    std::string line;
    std::size_t comparisons = 0;
    while (std::getline(golden, line)) {
        std::istringstream row(line);
        std::string name;
        char kind = 0;
        int seed = 0;
        int hasEnd = 0;
        std::string hexBits;
        std::ptrdiff_t expectedOffset = 0;
        int referenceError = 0;
        std::string fullBits;
        Require(static_cast<bool>(row >> name >> kind >> seed >> hasEnd >> hexBits >> expectedOffset
            >> referenceError >> fullBits), "complete reference row");
        const auto found = texts.find(name);
        Require(found != texts.end(), "reference input identity");
        const auto expectedBits = static_cast<std::uint32_t>(std::stoull(hexBits, nullptr, 16));
        constexpr int kReferenceRangeError = 34;
        const int expectedError = referenceError == kReferenceRangeError ? ERANGE : referenceError;
        const char* text = found->second->c_str();
        char* end = nullptr;
        errno = seed;
        const std::uint32_t actual = kind == 'u'
            ? CseLegacyStrtoul10(text, hasEnd ? &end : nullptr, profile)
            : static_cast<std::uint32_t>(CseLegacyStrtol10(text, hasEnd ? &end : nullptr, profile));
        const int actualError = errno;
        const std::ptrdiff_t actualOffset = hasEnd ? end - text : -1;
        if (actual != expectedBits || actualError != expectedError || actualOffset != expectedOffset) {
            std::fprintf(stderr, "%s kind=%c seed=%d end=%d actual=%08x/%td/%d expected=%08x/%td/%d\n",
                name.c_str(), kind, seed, hasEnd, actual, actualOffset, actualError,
                expectedBits, expectedOffset, expectedError);
            return 1;
        }
        ++comparisons;
    }
    Require(comparisons == cases.size() * 8, "every input/type/error/endptr combination compared");
    std::printf("PASS: %zu exact value/endptr/semantic-errno comparisons\n", comparisons);
    return 0;
}
