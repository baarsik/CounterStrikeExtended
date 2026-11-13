#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>
#include <SDL3/SDL_events.h>
#include "../../../src/common/sdl3_policy.h"

namespace {
struct KeyValues {
    KeyValues(const char* message, const char* key, int value) : message(message), key(key), value(value) {}
    const char* message;
    const char* key;
    int value;
};

class Panel {
public:
    void InternalKeyTyped(int unichar);
    bool IsKeyBoardInputEnabled() const { return enabled; }
    bool IsBuildGroupEnabled() const { return building; }
    void OnKeyTyped(wchar_t value) {
        ++typed;
        unit = static_cast<std::uint32_t>(value);
        units.push_back(unit);
    }
    void CallParentFunction(KeyValues* message) {
        ++forwarded;
        validMessage = std::strcmp(message->message, "KeyTyped") == 0
            && std::strcmp(message->key, "unichar") == 0;
        parentValue = message->value;
        parentUnits.push_back(static_cast<std::uint32_t>(parentValue));
        delete message;
    }
    bool enabled = true;
    bool building = false;
    int typed = 0;
    int forwarded = 0;
    bool validMessage = false;
    std::uint32_t unit = 0;
    int parentValue = 0;
    std::vector<std::uint32_t> units;
    std::vector<std::uint32_t> parentUnits;
};

#include "actual_keytyped.inc"

struct InputReceiver {
    Panel* panel;
    void InternalKeyTyped(wchar_t value) {
        panel->InternalKeyTyped(static_cast<int>(value));
    }
};
InputReceiver* g_InputInternal = nullptr;
#include "actual_sdl_text.inc"


void Require(bool condition) {
    if (!condition) {
        std::fputs("Actual panel text-unit dispatch mismatch\n", stderr);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    // Includes both individual surrogate units and values outside a UTF-16 unit.
    constexpr int kInputs[] = {0, 'A', 0x20ac, 0xd83d, 0xde00, 0xffff, 0x1f600, 0x10000, -1};
    for (int input : kInputs) {
        Panel enabled;
        enabled.InternalKeyTyped(input);
        Require(enabled.typed == 1 && enabled.forwarded == 0);
        Require(enabled.unit == static_cast<std::uint16_t>(input));
        Panel building;
        building.building = true;
        building.InternalKeyTyped(input);
        Require(building.typed == 0 && building.forwarded == 0);
        Panel disabled;
        disabled.enabled = false;
        disabled.InternalKeyTyped(input);
        Require(disabled.typed == 0 && disabled.forwarded == 1 && disabled.validMessage);
        Require(disabled.parentValue == input);
    }
    struct TextCase {
        const char* bytes;
        std::vector<std::uint32_t> units;
    };
    const TextCase textCases[] = {
        {"", {}}, {"A", {0x41}}, {"\xe2\x82\xac", {0x20ac}},
        {"\xf0\x9f\x99\x82", {0xd83d, 0xde42}},
        {"A\xf0\x9f\x99\x82" "B", {0x41, 0xd83d, 0xde42, 0x42}},
        {"\xf0\x90\x80\x80", {0xd800, 0xdc00}},
        {"\xf4\x8f\xbf\xbd", {0xdbff, 0xdffd}},
        {"\xed\xa0\xbd\xed\xb8\x80", {0xd83d, 0xde00}},
        {"\xef\xbf\xbf", {}}, {"\xed\xa0\x80", {}}, {"A\xc0\xaf" "B", {0x41}}
    };
    for (const TextCase& entry : textCases) {
        // The retained decoder can inspect up to six bytes; keep malformed input
        // controls in owned padded storage so this test isolates dispatch semantics.
        std::array<char, 32> padded{};
        Require(std::strlen(entry.bytes) < padded.size() - 6);
        std::memcpy(padded.data(), entry.bytes, std::strlen(entry.bytes));
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.text = padded.data();
        Panel panel;
        InputReceiver receiver{&panel};
        g_InputInternal = &receiver;
        DispatchActualSdlText(event);
        Require(panel.units == entry.units && panel.forwarded == 0);
        Panel disabled;
        disabled.enabled = false;
        receiver.panel = &disabled;
        DispatchActualSdlText(event);
        Require(disabled.units.empty() && disabled.parentUnits == entry.units);
    }
    g_InputInternal = nullptr;
    return EXIT_SUCCESS;
}
