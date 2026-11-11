#pragma once
#include "geometry_fixture.h"
#include "PHandle.h"
#include "vgui/IInputInternal.h"
#include "vgui/IVGui.h"
#include "check.h"

namespace input_retirement_fixture {
class Root final : public vgui2::Panel {
public:
    Root() : Panel(nullptr, "InputRetirementRoot") {}
    vgui2::VPANEL IsWithinTraverse(int, int, bool) override {
        ++traversals;
        return GetVPanel();
    }
    int traversals = 0;
};
inline void Run(vgui2::IVGui &gui, geometry_fixture::BaseUISurface &geometry, CreateInterfaceFn factory) {
    auto *input = static_cast<vgui2::IInputInternal *>(factory("VGUI_InputInternal001", nullptr));
    Require(input != nullptr, "actual input factory interface missing");
    geometry.forceCursorWithin = true;
    gui.RunFrame();
    Require(input->GetMouseOver() == 0, "partial initialization without root acquired mouse focus");
    auto *root = new Root;
    vgui2::DHANDLE<Root> handle;
    handle = root;
    root->SetSize(640, 480);
    geometry.Root(root->GetVPanel());
    geometry.forceCursorWithin = true;
    root->SetMouseInputEnabled(true);
    gui.RunFrame();
    Require(root->traversals > 0 && input->GetMouseOver() == root->GetVPanel(),
            "actual CVGui/input did not traverse valid embedded root through the injected surface");
    geometry.Root(0);
    gui.RunFrame();
    Require(handle.Get() != nullptr && input->GetMouseOver() == 0,
            "detachment failed to clear focus before PanelDeleted could mask an early return");
    root->MarkForDeletion();
    Require(handle.Get() != nullptr, "queued deletion completed before actual dispatch");
    // Original UpdateMouseFocus crashes here before dispatch: visible/within,
    // no input-context root, no eligible popup and detached embedded root.
    gui.RunFrame();
    Require(input->GetMouseOver() == 0, "detached-root turn retained stale mouse focus");
    Require(handle.Get() == nullptr, "detached-root turn failed to dispatch actual queued deletion");
    const auto noClient = gui.AllocPanel();
    geometry.InspectionRoot(noClient);
    // No-client is a partial-init control for the same actual input implementation.
    // It is not a solvable Panel tree, so only the actual input phase is invoked.
    input->UpdateMouseFocus(0, 0);
    Require(input->GetMouseOver() == 0, "missing client retained mouse focus");
    geometry.InspectionRoot(0);
    gui.FreePanel(noClient);
    gui.RunFrame();
    geometry.forceCursorWithin = false;
}
}
