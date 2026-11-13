#include "LocalizationCatalogue.h"
#include "LocalizationStorage.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

static std::vector<unsigned char> Encode(const std::u16string& text) {
    std::vector<unsigned char> bytes{0xff, 0xfe};
    for (const auto cell : text) {
        bytes.push_back(static_cast<unsigned char>(cell));
        bytes.push_back(static_cast<unsigned char>(cell >> 8));
    }
    return bytes;
}

int main(int argc, char** argv) {
    cse::localization::CatalogueParser parser;
    cse::localization::Catalogue catalogue;
    assert(parser.Parse(Encode(u"\"lang\" { \"Language\" \"English\" \"Tokens\" { \"Key\" \"Value\" } }"), catalogue));
    assert(catalogue.tokens.size() == 1 && catalogue.tokens[0].second == L"Value");
    assert(!parser.Parse(Encode(u"\"lang\" { \"Tokens\" { \"Key\" } }"), catalogue));
    assert(!parser.Parse(Encode(u"\"lang\" { \"Tokens\" { \"Key\" \"Value\" }"), catalogue));
    assert(!parser.Parse(Encode(u"\"lang\" { \"Tokens\" \"invalid\" }"), catalogue));
    assert(!parser.Parse(Encode(u"\"lang\" { \"Tokens\" {} } garbage"), catalogue));
    std::u16string invalid = u"\"lang\" { \"Tokens\" { \"Key\" \"";
    invalid.push_back(0xd800);
    invalid += u"\" } }";
    assert(!parser.Parse(Encode(invalid), catalogue));
    auto odd = Encode(u"\"lang\" { \"Tokens\" {} }");
    odd.push_back(0);
    assert(!parser.Parse(odd, catalogue));
    std::u16string longValue(8192, 'a');
    assert(parser.Parse(Encode(u"\"lang\" { \"Tokens\" { \"Key\" \"" + longValue + u"\" } }"), catalogue));
    assert(catalogue.tokens[0].second.size() == longValue.size());
    LocalizationStorage<char> storage;
    const int first = storage.AddMultipleToTail(4);
    std::memcpy(&storage[first], "old", 4);
    const char* pointer = &storage[first];
    for (int index = 0; index < 10000; ++index) {
        const int next = storage.AddMultipleToTail(2);
        storage[next] = 'x';
    }
    assert(pointer == &storage[first] && std::strcmp(pointer, "old") == 0);
    for (int index = 1; index < argc; ++index) {
        std::ifstream input(argv[index], std::ios::binary);
        std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (!parser.Parse(bytes, catalogue)) {
            std::cerr << "FAIL catalogue " << argv[index] << '\n';
            return 1;
        }
    }
    std::cout << "PASS catalogue parser/storage and " << argc - 1 << " actual files\n";
}
