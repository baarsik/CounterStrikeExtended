//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "smoked_game_layout.h"
#include "counterstrikeviewport.h"
#include "hud.h" // for gEngfuncs
#include "cstrikeclassmenu.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "game_controls/mouseoverpanelbutton.h"
#include "vgui_controls/Controls.h"
#include "Button.h"
#include "FileSystem.h"
#include <vector>
#include <stdio.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\VGUI\\cstrikeclassmenu.cpp" )

const char* GetResourceFile( int team )
{
	return team == 1 ? "Resource/UI/ClassMenu_TER.res" : "Resource/UI/ClassMenu_CT.res";
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClassMenu::CCSClassMenu( vgui2::Panel* parent ) : CClassMenu( parent, false )
{
	m_iFirst = 1;
	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
		SetScheme( "CareerClientScheme" );
	else
		SetScheme( "ClientScheme" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSClassMenu::~CCSClassMenu()
{
}

void CCSClassMenu::SetVisibleButton( const char* name, bool state )
{
	vgui2::Button* button = dynamic_cast<vgui2::Button*>( FindChildByName( name, false ) );
	if ( button )
	{
		button->SetVisible( state );
	}
}

void CCSClassMenu::Update( int*, int team )
{
	LoadControlSettings( GetResourceFile( team ), NULL );
	if ( g_iPlayerClass )
		SetVisibleButton( "CancelButton", true );
	else
		SetVisibleButton( "CancelButton", false );
	if ( !gHUD.IsGame( "czero" ) )
	{

		// if we don't have the new models installed,
		// turn off the militia and spetsnaz buttons
		SetVisibleButton( "militia", false );
		SetVisibleButton( "spetsnaz", false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: magic override to allow vgui to create mouse over buttons for us, and to use our class res files, not TFC's one
//-----------------------------------------------------------------------------
vgui2::Panel* CCSClassMenu::CreateControlByName( const char* controlName )
{
	if ( stricmp( "MouseOverPanelButton", controlName ) )
	{
		return CClassMenu::CreateControlByName( controlName );
	}
	ClassHelperPanel* panel = new ClassHelperPanel( this, NULL );
	panel->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	panel->SetBounds( x, y, wide, tall );
	MouseOverPanelButton* button = new MouseOverPanelButton( this, NULL, panel );
	panel->SetAssociatedButton( button );
	if ( m_iFirst )
	{
		m_iFirst = 0;
		m_pFirstButton = button; // this first button's page is triggered when the panel is set visible
	}
	return button;
}

CCSClassMenu::ClassHelperPanel::ClassHelperPanel( vgui2::Panel* parent, const char* name ) :
	vgui2::EditablePanel( parent, name )
{
	m_pAssociatedButton = NULL;
}

void CCSClassMenu::ClassHelperPanel::SetAssociatedButton( vgui2::Panel* button )
{
	m_pAssociatedButton = button;
}

vgui2::Panel* CCSClassMenu::ClassHelperPanel::GetAssociatedButton()
{
	return m_pAssociatedButton;
}


//-----------------------------------------------------------------------------
// Purpose: returns the filename of the class file for this class
//-----------------------------------------------------------------------------
const char* CCSClassMenu::ClassHelperPanel::GetClassPage( const char* className )
{
	static char classPanel[MAX_PATH];
	_snprintf( classPanel, sizeof( classPanel ), "classes/%s.res", className );
	if ( !vgui2::filesystem()->FileExists( classPanel ) )
	{
		if ( vgui2::filesystem()->FileExists( "classes/default.res" ) )
			_snprintf( classPanel, sizeof( classPanel ), "classes/default.res" );
		else
			return NULL;
	}
	return classPanel;
}

void CCSClassMenu::ClassHelperPanel::ApplySchemeSettings(vgui2::IScheme* scheme) {
    ASSERT_AT(GetAssociatedButton(), 135);
    const bool visible = IsVisible();
    LoadControlSettings(GetClassPage(GetAssociatedButton()->GetName()), nullptr);
    SetVisible(visible);
    vgui2::Panel::ApplySchemeSettings(scheme);
    for (const char* name : {"imageBG", "imageBorder"}) {
        if (auto* decoration = FindChildByName(name, false)) {
            vgui2::smoked::Decorations({decoration});
        }
    }
}

void CCSClassMenu::ApplySchemeSettings(vgui2::IScheme* scheme) {
    const bool restoreLayout = cse::ui::ControlsStyleChanged(this) && !cse::ui::ControlsSmokedGlass();
    CClassMenu::ApplySchemeSettings(scheme);
    if (!restoreLayout) {
        return;
    }
    // The viewport centers each fighter menu once on activation. Resource replay restores
    // its original canvas; restore the same letterbox offset and the unresourced helper anchors.
    constexpr int LegacyViewportWidth = 640;
    constexpr int LegacyViewportHeight = 480;
    constexpr float VerticalOffsetScale = 0.67f;
    int screenWidth = 0;
    int screenHeight = 0;
    vgui2::surface()->GetScreenSize(screenWidth, screenHeight);
    int x = 0;
    int y = 0;
    GetPos(x, y);
    const int offsetX = (screenWidth - GetAlternateProportionalValueFromNormal(LegacyViewportWidth)) / 2;
    const int offsetY = (screenHeight - GetAlternateProportionalValueFromNormal(LegacyViewportHeight)) / 2;
    SetPos(x + offsetX, y + static_cast<int>(offsetY * VerticalOffsetScale));
    m_pPanel->GetPos(x, y);
    for (int index = 0; index < GetChildCount(); ++index) {
        if (auto* detail = dynamic_cast<ClassHelperPanel*>(GetChild(index))) {
            detail->SetBounds(x, y, m_pPanel->GetWide(), m_pPanel->GetTall());
        }
    }
}

void CCSClassMenu::PerformLayout() {
    CClassMenu::PerformLayout();
    if (!cse::ui::ControlsSmokedGlass()) {
        // The shell resize can arrive after scheme replay. Keep every hover page on the
        // original ClassInfo anchor after the deferred autoresize has been processed.
        vgui2::smoked::RefreshResourceBounds(m_pPanel);
        int x = 0;
        int y = 0;
        m_pPanel->GetPos(x, y);
        for (int index = 0; index < GetChildCount(); ++index) {
            if (auto* detail = dynamic_cast<ClassHelperPanel*>(GetChild(index))) {
                detail->SetBounds(x, y, m_pPanel->GetWide(), m_pPanel->GetTall());
            }
        }
        return;
    }
    constexpr int MenuWidth = 920;
    constexpr int MenuHeight = 568;
    cse::gameui::Center(this, MenuWidth, MenuHeight);
    const int inset = vgui2::smoked::Pixels(cse::ui::WindowInset);
    const int contentTop = vgui2::smoked::Pixels(cse::ui::WindowHeaderHeight + cse::ui::WindowInset);
    const int gap = vgui2::smoked::Pixels(cse::gameui::ControlGap);
    const int columnWidth = (std::min)(vgui2::smoked::Pixels(cse::gameui::ActionColumnWidth),
        static_cast<int>(GetWide() * cse::gameui::MaximumActionWidthFraction));
    const int rowHeight = vgui2::smoked::Pixels(cse::ui::ControlHeight);
    cse::gameui::Title(FindChildByName("joinClass", false), GetWide());
    std::vector<vgui2::Panel*> actions;
    for (int index = 0; index < GetChildCount(); ++index) {
        auto* child = GetChild(index);
        if (dynamic_cast<vgui2::Button*>(child) && child->IsVisible()) {
            actions.push_back(child);
        }
    }
    const auto resourcePosition = [](vgui2::Panel* panel) {
        const auto found = cse::ui::ControlsResourceGeometry().find(panel);
        return found == cse::ui::ControlsResourceGeometry().end() ? 0 : std::atoi(found->second.y.c_str());
    };
    std::stable_sort(actions.begin(), actions.end(), [&](auto* left, auto* right) {
        return resourcePosition(left) < resourcePosition(right);
    });
    int actionY = contentTop;
    for (auto* action : actions) {
        const int y = !stricmp(action->GetName(), "CancelButton") ? GetTall() - inset - rowHeight : actionY;
        action->SetBounds(inset, y, columnWidth, rowHeight);
        actionY += rowHeight + gap;
    }
    const int detailX = inset + columnWidth + gap;
    const int detailWidth = GetWide() - detailX - inset;
    const int detailHeight = GetTall() - contentTop - inset;
    m_pPanel->SetBounds(detailX, contentTop, detailWidth, detailHeight);
    for (int index = 0; index < GetChildCount(); ++index) {
        if (auto* detail = dynamic_cast<ClassHelperPanel*>(GetChild(index))) {
            detail->SetBounds(detailX, contentTop, detailWidth, detailHeight);
        }
    }
}

void CCSClassMenu::PaintBackground() {
    if (cse::ui::ControlsSmokedGlass()) {
        vgui2::smoked::Surface(this);
        return;
    }
    CClassMenu::PaintBackground();
}

void CCSClassMenu::ClassHelperPanel::PerformLayout() {
    vgui2::EditablePanel::PerformLayout();
    if (!cse::ui::ControlsSmokedGlass()) {
        return;
    }
    constexpr float ClassCanvasWidth = 300.0f;
    constexpr float ClassCanvasHeight = 308.0f;
    constexpr float MaximumPreviewScale = 1.5f;
    constexpr int DescriptionFontHeight = 16;
    const float scale = (std::min)({GetWide() / ClassCanvasWidth, GetTall() / ClassCanvasHeight,
        MaximumPreviewScale});
    const auto pixels = [scale](int value) { return static_cast<int>(std::lround(value * scale)); };
    const int canvasX = (GetWide() - pixels(static_cast<int>(ClassCanvasWidth))) / 2;
    for (int index = 0; index < GetChildCount(); ++index) {
        auto* child = GetChild(index);
        const auto found = cse::ui::ControlsResourceGeometry().find(child);
        if (found == cse::ui::ControlsResourceGeometry().end()) {
            continue;
        }
        const auto& source = found->second;
        int x = canvasX + pixels(std::atoi(source.x.c_str()));
        const int width = pixels(source.width);
        if (!stricmp(child->GetName(), "classimage")) {
            x = (GetWide() - width) / 2;
        }
        child->SetBounds(x, pixels(std::atoi(source.y.c_str())), width, pixels(source.height));
        if (auto* label = dynamic_cast<vgui2::Label*>(child)) {
            label->SetFont(vgui2::smoked::TextFont(label, vgui2::scheme()->GetIScheme(label->GetScheme()),
                label->GetFont(), true, vgui2::smoked::Pixels(DescriptionFontHeight)));
        }
    }
}
