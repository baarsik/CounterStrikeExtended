#include <cstdlib>
#include <limits>

#include "money_hud_layout.h"

int main() {
    if (MoneyHudDigitSlots(0) != 5 || MoneyHudDigitSlots(16000) != 5 || MoneyHudDigitSlots(99999) != 5 ||
        MoneyHudDigitSlots(100000) != 6 || MoneyHudDigitSlots(999999) != 6 ||
        MoneyHudDigitSlots((std::numeric_limits<int>::min)()) != 10) {
        std::abort();
    }
}
