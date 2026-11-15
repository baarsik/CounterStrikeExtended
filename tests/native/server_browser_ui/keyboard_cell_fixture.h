#pragma once

#include "PropertyPage.h"
#include "SectionedListPanel.h"
#include "Button.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "check.h"

namespace keyboard_cell_fixture {
// Capture and selected-row identity are the Keyboard owner's external inputs.
// The row storage, cell measurement, focus forwarding and key handler are production code.
class BindingList : public vgui2::SectionedListPanel {
public:
    explicit BindingList(vgui2::Panel* parent) : SectionedListPanel(parent, "Bindings") {}
    bool IsCapturing() { return false; }
    int GetItemOfInterest() { return GetSelectedItem(); }
};
class COptionsSubKeyboard : public vgui2::PropertyPage {
public:
    using BaseClass = vgui2::PropertyPage;
    explicit COptionsSubKeyboard(vgui2::Panel* parent) : PropertyPage(parent, "KeyboardCellOwner", true) {
        m_pKeyBindList = new BindingList(this);
        m_pClearBindingButton = new vgui2::Button(this, "Clear", "Clear");
    }
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void RemoveKeyFromBindItems(const char*) { ++removedBindings; }
    BindingList* m_pKeyBindList;
    vgui2::Button* m_pClearBindingButton;
    int removedBindings = 0;
};
#include "keyboard_pressed_owner.inc"

inline void Run(vgui2::Panel* root, vgui2::HScheme scheme, float& style) {
    const float previous = style;
    for (float selection : {0.0f, 1.0f}) {
        style = selection;
        COptionsSubKeyboard page(root);
        page.SetScheme(scheme);
        page.SetBounds(0, 0, 420, 240);
        page.MakeReadyForUse();
        BindingList* list = page.m_pKeyBindList;
        list->SetBounds(0, 0, 400, 200);
        list->AddSection(0, "Keyboard");
        list->AddColumnToSection(0, "Action", "Action", 0, 180);
        list->AddColumnToSection(0, "Key", "Key", 0, 90);
        KeyValues* data = new KeyValues("Binding");
        data->SetString("Action", "Move forward");
        data->SetString("Key", "W");
        data->SetString("AltKey", "UPARROW");
        const int row = list->AddItem(0, data);
        data->deleteThis();
        list->SetSelectedItem(row);
        // No SolveTraverse/Paint between row creation and the first physical-key handler.
        page.OnKeyCodePressed(vgui2::KEY_LSHIFT);
        page.OnKeyCodePressed(vgui2::KEY_RSHIFT);
        Require(page.removedBindings == 0, "Shift preserves existing key bindings before first row paint");
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        Require(list->GetCellBounds(row, 1, x, y, width, height) && x == 180 && width == 90,
            "unpainted binding cells have valid column bounds");
        page.OnKeyCodePressed(vgui2::KEY_DELETE);
        Require(page.removedBindings == 2, "Delete still reaches both real binding-removal requests");
        list->InvalidateLayout(true, true);
        Require(list->GetCellBounds(row, 1, x, y, width, height) && x == 180 && width == 90,
            "binding cell bounds survive scheme refresh without a paint pass");
        list->InvalidateItem(row);
        page.OnKeyCodePressed(vgui2::KEY_LSHIFT);
    }
    style = previous;
}
} // namespace keyboard_cell_fixture
