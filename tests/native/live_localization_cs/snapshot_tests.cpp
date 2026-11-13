#include "localization_message_snapshot.h"
#include <cassert>
#include <cstring>
#include <string>
#include <utility>

struct Message {
    int effect;
    float holdtime;
    const char* pName;
    const char* pMessage;
};
static Message translated{99, 400.0f, "Tip", "Nowa wskazowka"};
static Message* Lookup(const char* name) {
    return std::strcmp(name, "Tip") == 0 ? &translated : nullptr;
}

int main() {
    cse::localization::MessageSnapshot<Message> snapshot;
    assert(snapshot.Get() == nullptr);
    {
        std::string name = "Tip";
        std::string text = "Original hint";
        Message original{2, 5.0f, name.c_str(), text.c_str()};
        snapshot.Assign(&original, true);
        original.effect = 7;
    }
    assert(std::strcmp(snapshot.Get()->pName, "Tip") == 0);
    assert(std::strcmp(snapshot.Get()->pMessage, "Original hint") == 0);
    assert(snapshot.Get()->effect == 2);
    snapshot.Get()->holdtime = 6.0f;
    assert(snapshot.Relocalize(Lookup));
    assert(std::strcmp(snapshot.Get()->pMessage, "Nowa wskazowka") == 0);
    assert(snapshot.Get()->effect == 2 && snapshot.Get()->holdtime == 6.0f);
    auto copied = snapshot;
    auto moved = std::move(snapshot);
    snapshot.Clear();
    assert(std::strcmp(copied.Get()->pMessage, "Nowa wskazowka") == 0);
    assert(std::strcmp(moved.Get()->pName, "Tip") == 0);
    copied = copied;
    moved = std::move(moved);
    assert(std::strcmp(moved.Get()->pMessage, "Nowa wskazowka") == 0);
    for (const char* name : {"Tip", "__NETMESSAGE__", "__DEMOMESSAGE__", "#player"}) {
        Message literal{1, 9.0f, name, "#Tip"};
        snapshot.Assign(&literal, false);
        assert(!snapshot.Relocalize(Lookup));
        assert(!snapshot.HasCatalogueSource());
        assert(std::strcmp(snapshot.Get()->pMessage, "#Tip") == 0);
    }
    Message unknown{1, 2.0f, "Missing", "fallback"};
    snapshot.Assign(&unknown, true);
    assert(!snapshot.Relocalize(Lookup));
    assert(std::strcmp(snapshot.Get()->pMessage, "fallback") == 0);
    Message empty{0, 0.0f, nullptr, nullptr};
    snapshot.Assign(&empty, true);
    assert(snapshot.Get()->pName == nullptr && snapshot.Get()->pMessage == nullptr);
    snapshot.Assign(nullptr, false);
    assert(snapshot.Get() == nullptr);
}
