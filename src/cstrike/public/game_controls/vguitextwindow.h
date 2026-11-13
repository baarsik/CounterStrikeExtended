//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include <string>

namespace vgui2
{
class RichText;
class Button;
}

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------
class CVGUITextWindow : public vgui2::Frame
{
public:
	CVGUITextWindow( vgui2::Panel* parent );
	void OnCommand( const char* command ) override;
    void ActivateLocalized(const char* title, const char* message);
    void SetFileSource(const char* filename);
    void RefreshLanguage();
	void Activate( const char* title, const char* msg );
	void Activate( const wchar_t* title, const wchar_t* msg );

private:
    std::string m_localizedTitle;
    std::string m_localizedMessage;
    std::string m_fileSource;
	vgui2::RichText*	m_pMessage;
	vgui2::Button*	m_pOK;
};
