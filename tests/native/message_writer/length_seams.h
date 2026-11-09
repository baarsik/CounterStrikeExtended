#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Only reported length is controlled for the large-native-size cases. Ordinary
// encoded message cases use the real CRT strlen; every copy uses actual strcpy.
std::size_t CseFixtureLength(const char* value);
