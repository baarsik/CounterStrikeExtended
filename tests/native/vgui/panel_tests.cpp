#include <array>
#include <cstdint>
#include <cstdio>
#include <new>
#include "../strings/high_offset_buffer.h"
#ifdef GetClassName
#undef GetClassName
#endif
#ifdef SendMessage
#undef SendMessage
#endif
#include "VPanel.h"
#include "vgui/IClientPanel.h"

namespace vgui2 {
class ISurface;
ISurface *g_pSurface = nullptr;
}

#include "client_spy.h"

// The bounded fixture supplies registry slots; conversions are the production CVGui methods.
struct RegistrySlot {
    unsigned short m_iSerialNumber = 0;
    vgui2::VPANEL m_pPanel = 0;
    bool active = false;
};
struct Registry {
    std::array<RegistrySlot, 3> slots{};
    bool IsValidIndex(unsigned short index) const {
        return index < slots.size() && slots[index].active;
    }
    RegistrySlot &operator[](unsigned short index) {
        return slots[index];
    }
};
class CVGui {
public:
    Registry m_PanelList;
    vgui2::HPanel PanelToHandle(vgui2::VPANEL panel);
    vgui2::VPANEL HandleToPanel(vgui2::HPanel index);
};
#define INVALID_VGUI_PANEL static_cast<vgui2::HPanel>(UINT32_MAX)
#include "../../../src/vgui2/src/panel_handles_impl.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "VGUI panel check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

int main() {
    HighOffsetBuffer mapping(nullptr);
    CHECK(mapping.get());
    auto *panels = reinterpret_cast<vgui2::VPanel *>(mapping.get());
    constexpr int PanelCount = 3;
    for (int index = 0; index < PanelCount; ++index) {
        new (panels + index) vgui2::VPanel;
    }
    ClientSpy firstClient;
    ClientSpy secondClient;
    panels[0].Init(&firstClient);
    panels[2].Init(&secondClient);
    const auto childHandle = reinterpret_cast<vgui2::VPANEL>(panels + 1);
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(childHandle > UINT32_MAX);
    }
    CVGui gui;
    constexpr unsigned short SlotIndex = 1;
    constexpr unsigned short LastSerial = UINT16_MAX;
    panels[1].SetListEntry(SlotIndex);
    gui.m_PanelList[SlotIndex] = {LastSerial, childHandle, true};
    const auto handle = gui.PanelToHandle(childHandle);
    CHECK(handle == UINT32_C(0x0001ffff));
    CHECK(gui.HandleToPanel(handle) == childHandle);
    CHECK(gui.PanelToHandle(0) == INVALID_VGUI_PANEL);
    CHECK(gui.HandleToPanel(INVALID_VGUI_PANEL) == 0);
    CHECK(gui.HandleToPanel(UINT32_C(0x00020000)) == 0);
    gui.m_PanelList[SlotIndex].active = false;
    CHECK(gui.HandleToPanel(handle) == 0);
    gui.m_PanelList[SlotIndex] = {0, childHandle, true};
    CHECK(gui.HandleToPanel(handle) == 0);
    CHECK(gui.PanelToHandle(childHandle) == UINT32_C(0x00010000));
    CHECK(gui.HandleToPanel(gui.PanelToHandle(childHandle)) == childHandle);
    panels[1].SetParent(panels);
    CHECK(firstClient.childAdded == childHandle);
    CHECK(panels[0].GetChildCount() == 1 && panels[0].GetChild(0) == panels + 1);
    CHECK(panels[1].GetParent() == panels);
    panels[1].SetParent(panels + 2);
    CHECK(secondClient.childAdded == childHandle && panels[0].GetChildCount() == 0);
    CHECK(panels[2].GetChildCount() == 1 && panels[1].HasParent(panels + 2));
    auto *message = reinterpret_cast<KeyValues *>(panels); // Opaque payload; the production forwarder never reads it.
    panels[0].SendMessage(message, childHandle);
    CHECK(firstClient.message == message && firstClient.messageFrom == childHandle);
    panels[1].SetParent(nullptr);
    CHECK(panels[2].GetChildCount() == 0 && panels[1].GetParent() == nullptr);
    for (int index = PanelCount; index > 0; --index) {
        panels[index - 1].~VPanel();
    }
    std::puts("Production VPanel parenting and callback identities passed at native high addresses");
    return 0;
}
