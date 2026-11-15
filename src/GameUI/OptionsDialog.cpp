#include "../vgui2/controls/SmokedGlass.h"
//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "OptionsDialog.h"

#include "GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"
#include "vgui/KeyValues.h"
#include "ModInfo.h"
#include "MultiplayerAdvancedDialog.h"
#include "OptionsSubAdvanced.h"
#include "OptionsSubAudio.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubInterface.h"
#include "OptionsSubMouse.h"
#include "OptionsSubMultiplayer.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "Taskbar.h"

#include "PropertySheet.h"
#include "Button.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include <algorithm>
#include "vgui_controls/Controls.h"

#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog(vgui2::Panel* parent)
    : vgui2::PropertyDialog(parent, "OptionsDialog") {
    SetHDProportional(true);
    constexpr int DefaultHeight = 444;
    constexpr int DefaultWidth = 522;
    const int tall = vgui2::scheme()->GetHDProportionalScaledValue(DefaultHeight);
    const int wide = GetProportionalScaledValue(DefaultWidth);
    SetBounds(0, 0, wide, tall);
    SetCloseSound("sound/UI/launch_dnmenu1.wav");
    g_pTaskbar->AddTask(GetVPanel());
    SetTitle("#GameUI_Options", true);

    // PageTab captures the minimum width when AddPage creates it.
    constexpr int MinimumTabWidth = 24;
    GetPropertySheet()->SetTabWidth(GetProportionalScaledValue(MinimumTabWidth));
    if (ModInfo()->IsMultiplayerOnly() && !ModInfo()->IsSinglePlayerOnly()) {
        AddPage(new COptionsSubMultiplayer(this), "#GameUI_Multiplayer");
    }
    AddPage(new COptionsSubAudio(this), "#GameUI_Audio");
    AddPage(new COptionsSubVideo(this), "#GameUI_Video");
    m_interfacePage = new COptionsSubInterface(this);
    AddPage(m_interfacePage, "#CSEInterface_Tab");
    AddPage(new COptionsSubMouse(this), "#GameUI_Mouse");
    AddPage(new COptionsSubKeyboard(this), "#GameUI_Keyboard");
    if (!ModInfo()->IsSinglePlayerOnly() || !stricmp(ModInfo()->GetGameDescription(), "Half-Life")) {
        AddPage(new COptionsSubVoice(this), "#GameUI_Voice");
    }
    AddPage(new COptionsSubAdvanced(this), "#GameUI_Content");
    if (ModInfo()->IsSinglePlayerOnly() && stricmp(ModInfo()->GetGameDescription(), "Half-Life")) {
        AddPage(new CMultiplayerAdvancedPage(this), "#GameUI_AdvancedNoEllipsis");
    }
    if ((!ModInfo()->IsMultiplayerOnly() && !ModInfo()->IsSinglePlayerOnly())
        || !stricmp(ModInfo()->GetGameDescription(), "Half-Life")) {
        AddPage(new COptionsSubMultiplayer(this), "#GameUI_Multiplayer");
    }
    SetApplyButtonVisible(true);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Overrides the base class so it can setup the taskbar title properly
//-----------------------------------------------------------------------------
void COptionsDialog::SetTitle( const char* title, bool surfaceTitle )
{
	vgui2::Frame::SetTitle( title, surfaceTitle );
	if ( !g_pTaskbar )
		return;

	wchar_t titleBuffer[1024];
	wchar_t* localizedTitle = vgui2::localize()->Find( title );
	if ( !localizedTitle )
	{
		vgui2::localize()->ConvertANSIToUnicode( title, titleBuffer, sizeof( titleBuffer ) );
		localizedTitle = titleBuffer;
	}

	g_pTaskbar->SetTitle( GetVPanel(), localizedTitle );
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
    if (cse::ui::ControlsSmokedGlass()) {
        // A hidden MakeReadyForUse pass applies schemes but does not run layout.
        // Select the first page before the window receives its first keyboard focus request.
        vgui2::PropertySheet* sheet = GetPropertySheet();
        if (!sheet->GetActivePage() && sheet->GetNumPages() > 0) {
            sheet->SetActivePage(sheet->GetPage(0));
        }
        MakeReadyForUse();
        PerformLayout();
    }
	vgui2::Frame::Activate();
	EnableApplyButton( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle( "#GameUI_Options", true );
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog on close
//-----------------------------------------------------------------------------
void COptionsDialog::OnClose() {
    if (m_languageApplyPending) {
        return;
    }
    vgui2::Frame::OnClose();
    MarkForDeletion();
}

int COptionsDialog::StyledHeight() {
    int workspaceX = 0;
    int workspaceY = 0;
    int workspaceWide = 0;
    int workspaceTall = 0;
    vgui2::surface()->GetWorkspaceBounds(workspaceX, workspaceY, workspaceWide, workspaceTall);
    constexpr int ScreenMargin = 4;
    constexpr int DefaultStyledHeight = 640;
    const int desired = vgui2::smoked::Pixels(DefaultStyledHeight);
    return std::min(desired, workspaceTall - 2 * GetProportionalScaledValue(ScreenMargin));
}

void COptionsDialog::PerformLayout() {
    const bool smoked = cse::ui::ControlsSmokedGlass();
    const bool materialLayout = smoked || m_originalStyleHeight != 0;
    const int oldWidth = GetWide();
    const int oldHeight = GetTall();
    int oldX = 0;
    int oldY = 0;
    GetPos(oldX, oldY);
    if (smoked) {
        if (!m_originalStyleHeight) {
            m_originalStyleHeight = GetTall();
        }
        const int desiredHeight = StyledHeight();
        SetTall(desiredHeight);
    } else if (m_originalStyleHeight) {
        SetTall(m_originalStyleHeight);
        m_originalStyleHeight = 0;
    }
    vgui2::PropertyDialog::PerformLayout();
    vgui2::PropertySheet* sheet = GetPropertySheet();
    int tabsWidth = 0;
    for (int index = 0; index < sheet->GetChildCount(); ++index) {
        vgui2::Panel* child = sheet->GetChild(index);
        if (strcmp(child->GetName(), "tab") == 0) {
            constexpr int TabSpacing = 1;
            tabsWidth += child->GetWide() + TabSpacing;
        }
    }
    constexpr int DefaultWidth = 522;
    constexpr int OriginalScreenMargin = 8;
    constexpr int StyledScreenMargin = 4;
    int workspaceX = 0;
    int workspaceY = 0;
    int workspaceWidth = 0;
    int workspaceHeight = 0;
    vgui2::surface()->GetWorkspaceBounds(workspaceX, workspaceY, workspaceWidth, workspaceHeight);
    const int frameInset = GetWide() - sheet->GetWide();
    // Wrapped tabs never determine owner geometry, including when the active page changes.
    constexpr int DefaultStyledWidth = 760;
    const int requiredWidth = smoked ? vgui2::smoked::Pixels(DefaultStyledWidth)
        : std::max(GetProportionalScaledValue(DefaultWidth), tabsWidth + frameInset);
    const int screenMargin = smoked ? vgui2::smoked::Pixels(StyledScreenMargin)
        : GetProportionalScaledValue(OriginalScreenMargin);
    const int availableWidth = std::max(1, workspaceWidth - 2 * screenMargin);
    const int width = std::min(requiredWidth, availableWidth);
    if (width != GetWide()) {
        SetWide(width);
        vgui2::PropertyDialog::PerformLayout();
    }

    constexpr int CenterTolerance = 1;
    const bool wasCentered = std::abs(oldX + oldWidth / 2 - workspaceX - workspaceWidth / 2) <= CenterTolerance &&
        std::abs(oldY + oldHeight / 2 - workspaceY - workspaceHeight / 2) <= CenterTolerance;
    if (materialLayout && wasCentered && (oldWidth != GetWide() || oldHeight != GetTall())) {
        MoveToCenterOfScreen();
    }
    if (smoked) {
        return;
    }

    int clientX = 0;
    int clientY = 0;
    int clientWidth = 0;
    int clientHeight = 0;
    GetClientArea(clientX, clientY, clientWidth, clientHeight);
    constexpr int ButtonGap = 8;
    constexpr int ButtonPadding = 12;
    constexpr int MinimumButtonWidth = 72;
    int right = clientX + clientWidth - GetProportionalScaledValue(ButtonGap);
    for (const char* name : {"ApplyButton", "CancelButton", "OKButton"}) {
        auto* button = dynamic_cast<vgui2::Button*>(FindChildByName(name, false));
        if (!button || !button->IsVisible()) {
            continue;
        }
        int contentWidth = 0;
        int contentHeight = 0;
        button->GetContentSize(contentWidth, contentHeight);
        const int buttonWidth = std::max(GetProportionalScaledValue(MinimumButtonWidth),
            contentWidth + GetProportionalScaledValue(ButtonPadding));
        int buttonX = 0;
        int buttonY = 0;
        button->GetPos(buttonX, buttonY);
        button->SetBounds(right - buttonWidth, buttonY, buttonWidth, button->GetTall());
        right -= buttonWidth + GetProportionalScaledValue(ButtonGap);
    }
}

void COptionsDialog::OnCommand(const char* command) {
    if (m_languageApplyPending) {
        return;
    }
    const bool appliesChanges = stricmp(command, "Apply") == 0 || stricmp(command, "OK") == 0;
    if (appliesChanges) {
        if (!m_interfacePage->ApplyLanguage()) {
            GetPropertySheet()->SetActivePage(m_interfacePage);
            EnableApplyButton(true);
            return;
        }
        if (m_interfacePage->IsLanguagePending()) {
            m_languageApplyPending = true;
            m_closeAfterLanguageApply = stricmp(command, "OK") == 0;
            // Apply the other pages once. Keep this owner alive until the engine
            // reports the transaction result, including when OK requested closure.
            vgui2::PropertyDialog::OnCommand("Apply");
            SetEnabled(false);
            return;
        }
    }
    vgui2::PropertyDialog::OnCommand(command);
}

void COptionsDialog::OnThink() {
    vgui2::PropertyDialog::OnThink();
    if (cse::ui::ControlsSmokedGlass() && m_originalStyleHeight) {
        const int desiredHeight = StyledHeight();
        if (GetTall() != desiredHeight) {
            InvalidateLayout(true, false);
        }
    }
    if (!m_languageApplyPending) {
        return;
    }
    bool applied = false;
    if (!m_interfacePage->CompleteLanguageRequest(applied)) {
        return;
    }
    m_languageApplyPending = false;
    SetEnabled(true);
    if (!applied) {
        m_closeAfterLanguageApply = false;
        GetPropertySheet()->SetActivePage(m_interfacePage);
        EnableApplyButton(true);
        return;
    }
    if (m_closeAfterLanguageApply) {
        m_closeAfterLanguageApply = false;
        OnClose();
    }
}
