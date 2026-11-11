#include <cstdio>
#include <cstdlib>

namespace {
int baseDestructions = 0;
int derivedDestructions = 0;
int constructions = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

class CBotManager {
public:
    virtual void Tick() = 0;
    ~CBotManager() {
        Require(derivedDestructions == baseDestructions + 1, "derived resources retire before base storage");
        ++baseDestructions;
    }
};

class CCSBotManager : public CBotManager {
public:
    CCSBotManager() {
        ++constructions;
    }
    ~CCSBotManager() {
        ++derivedDestructions;
    }
    void Tick() override {}
};

CBotManager* TheBots = nullptr;
}

#include "install.inc"

int main() {
    constexpr int MapCount = 3;
    for (int map = 0; map < MapCount; ++map) {
        InstallBotControl();
        Require(TheBots && constructions == map + 1, "one live manager for every map");
        Require(baseDestructions == map && derivedDestructions == map, "previous manager fully retired");
    }
    delete static_cast<CCSBotManager*>(TheBots);
    TheBots = nullptr;
    Require(baseDestructions == MapCount && derivedDestructions == MapCount, "all manager resources retired");
}
