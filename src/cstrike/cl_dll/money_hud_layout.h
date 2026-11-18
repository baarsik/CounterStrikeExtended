#pragma once

#include <cstdint>

inline int MoneyHudDigitSlots(int amount) {
    constexpr int kClassicMoneyDigits = 5;
    std::uint64_t magnitude = amount < 0 ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(amount))
                                         : static_cast<std::uint64_t>(amount);
    int digits = 1;
    while (magnitude >= 10) {
        magnitude /= 10;
        ++digits;
    }
    // DrawHudNumber skips the minus sign before advancing through digit sprites.
    return digits > kClassicMoneyDigits ? digits : kClassicMoneyDigits;
}
