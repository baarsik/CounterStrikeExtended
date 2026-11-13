#include "resource_text.h"

#include <cassert>
#include <string>

int main() {
    std::string output = "old text";
    assert(cse::resource_text::Decode(nullptr, 0, output) && output.empty());
    assert(!cse::resource_text::Decode(nullptr, 1, output) && output.empty());

    const std::string legacy = "caf\xe9";
    assert(cse::resource_text::Decode(legacy.data(), legacy.size(), output) && output == legacy);
    const std::string utf8 = "\xef\xbb\xbf" "caf\xc3\xa9";
    assert(cse::resource_text::Decode(utf8.data(), utf8.size(), output) && output == "caf\xc3\xa9");

    const unsigned char littleEndian[] = {0xff, 0xfe, 0x41, 0, 0x2d, 0x4e, 0x3d, 0xd8, 0x80, 0xde};
    const unsigned char bigEndian[] = {0xfe, 0xff, 0, 0x41, 0x4e, 0x2d, 0xd8, 0x3d, 0xde, 0x80};
    const std::string expected = "A\xe4\xb8\xad\xf0\x9f\x9a\x80";
    assert(cse::resource_text::Decode(littleEndian, sizeof(littleEndian), output) && output == expected);
    assert(cse::resource_text::Decode(bigEndian, sizeof(bigEndian), output) && output == expected);
    assert(cse::resource_text::Decode(littleEndian, 2, output) && output.empty());
    assert(!cse::resource_text::Decode(littleEndian, sizeof(littleEndian) - 1, output) && output.empty());

    const unsigned char unpairedSurrogate[] = {0xff, 0xfe, 0x3d, 0xd8};
    assert(cse::resource_text::Decode(unpairedSurrogate, sizeof(unpairedSurrogate), output)
        && output == "\xef\xbf\xbd");
}
