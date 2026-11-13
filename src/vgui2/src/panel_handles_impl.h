#pragma once

// Packed handles retain the legacy 16-bit list index and 16-bit reuse serial.
constexpr unsigned int CsePanelSerialBits = 16;

vgui2::HPanel CVGui::PanelToHandle(vgui2::VPANEL panel) {
    if (panel) {
        const auto listEntry = reinterpret_cast<vgui2::VPanel *>(panel)->GetListEntry();
        return (static_cast<vgui2::HPanel>(listEntry) << CsePanelSerialBits)
            + m_PanelList[listEntry].m_iSerialNumber;
    }
    return INVALID_VGUI_PANEL;
}

vgui2::VPANEL CVGui::HandleToPanel(vgui2::HPanel index) {
    if (index == INVALID_VGUI_PANEL) {
        return 0;
    }
    const auto listEntry = static_cast<unsigned short>(index >> CsePanelSerialBits);
    if (m_PanelList.IsValidIndex(listEntry)
        && m_PanelList[listEntry].m_iSerialNumber == static_cast<unsigned short>(index)) {
        return m_PanelList[listEntry].m_pPanel;
    }
    return 0;
}
