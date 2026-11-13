//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyDialog.h"

class COptionsSubInterface;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class COptionsDialog : public vgui2::PropertyDialog
{
    DECLARE_CLASS_SIMPLE_NAMED(COptionsDialog, vgui2::PropertyDialog, "PropertyDialog");
public:
	COptionsDialog( vgui2::Panel* parent );
	~COptionsDialog() override;

	void Run();
	void Activate() override;

protected:
    void PerformLayout() override;
    void OnCommand(const char* command) override;
    MESSAGE_FUNC(OnRestartForLanguageChange, "RestartForLanguageChange");
	void SetTitle( const char* title, bool surfaceTitle ) override;
	void OnClose() override;

private:
    COptionsSubInterface* m_interfacePage = nullptr;
    bool m_languageRestartPending = false;
};
