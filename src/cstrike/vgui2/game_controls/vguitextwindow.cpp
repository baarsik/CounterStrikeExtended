//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "vguitextwindow.h"
#include "../../../vgui2/controls/RichText.h"
#include "../../../vgui2/controls/Button.h"
#include "iviewport.h"
#include "../../../common/resource_text.h"
#include "../../../vgui2/controls/ScrollBar.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include <vector>
#include <climits>
#include <string.h>

CVGUITextWindow::CVGUITextWindow( vgui2::Panel* parent ) : vgui2::Frame( parent, "VGUITextWindow", true, false )
{
	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );

	// hide the system buttons
	SetTitleBarVisible( false );
	m_pMessage = new vgui2::RichText( this, "Message", true );
	m_pOK = new vgui2::Button( this, "ok", "#PropertyDialog_OK" );
	m_pOK->SetCommand( "okay" );
	LoadControlSettings( "Resource/UI/TextWindow.res", NULL );
	InvalidateLayout( false, false );
}

void CVGUITextWindow::OnCommand( const char* command )
{
	if ( !_stricmp( command, "okay" ) )
	{
		Close();
		gViewPortInterface->HideBackGround();
	}
	vgui2::Frame::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: shows the MOTD
//-----------------------------------------------------------------------------
void CVGUITextWindow::Activate( const char* title, const char* message )
{
	m_localizedTitle.clear();
    m_localizedMessage.clear();
    m_fileSource.clear();
    vgui2::Frame::Activate();
	std::vector<wchar_t> literalTitle(strlen(title ? title : "") + 1);
    vgui2::localize()->ConvertANSIToUnicode(title ? title : "", literalTitle.data(),
        static_cast<int>(literalTitle.size() * sizeof(wchar_t)));
    SetTitle(literalTitle.data(), false);
    std::vector<wchar_t> literalMessage(strlen(message ? message : "") + 1);
    vgui2::localize()->ConvertANSIToUnicode(message ? message : "", literalMessage.data(),
        static_cast<int>(literalMessage.size() * sizeof(wchar_t)));
    m_pMessage->SetText(literalMessage.data());
	m_pMessage->GotoTextStart();
	SetVisible( true );
}

void CVGUITextWindow::Activate( const wchar_t* title, const wchar_t* message )
{
	m_localizedTitle.clear();
    m_localizedMessage.clear();
    m_fileSource.clear();
    vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pMessage->SetText( message );
	m_pMessage->GotoTextStart();
	SetVisible( true );
}

void CVGUITextWindow::ActivateLocalized(const char* title, const char* message) {
    Activate(title, message);
    m_localizedTitle = title ? title : "";
    m_localizedMessage = message ? message : "";
    RefreshLanguage();
}

void CVGUITextWindow::SetFileSource(const char* filename) {
    m_fileSource = filename ? filename : "";
}

void CVGUITextWindow::RefreshLanguage() {
    auto* scroll = dynamic_cast<vgui2::ScrollBar*>(m_pMessage->FindChildByName("ScrollBar", false));
    const int position = scroll ? scroll->GetValue() : 0;
    if (!m_localizedTitle.empty()) {
        SetTitle(m_localizedTitle.c_str(), false);
    }
    if (!m_localizedMessage.empty()) {
        m_pMessage->SetTextPreservingView(m_localizedMessage.c_str());
    }
    if (!m_fileSource.empty()) {
        int size = 0;
        void* file = gViewPortInterface->GetClientDllInterface()->COM_LoadFile(
            const_cast<char*>(m_fileSource.c_str()), 5, &size);
        std::string text;
        const bool decoded = file && size >= 0
            && cse::resource_text::Decode(file, static_cast<std::size_t>(size), text);
        if (file) {
            gViewPortInterface->GetClientDllInterface()->COM_FreeFile(file);
        }
        if (decoded) {
            std::vector<wchar_t> unicode(text.size() + 1);
            vgui2::localize()->ConvertANSIToUnicode(text.c_str(), unicode.data(),
                static_cast<int>(unicode.size() * sizeof(wchar_t)));
            m_pMessage->SetTextPreservingView(unicode.data());
        }
    }
    m_pMessage->InvalidateLayout(true, false);
    if (scroll) {
        scroll->SetValue(position);
    }
}
