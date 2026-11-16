//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BUYMENU_H
#define BUYMENU_H

#include "WizardPanel.h"

class CBuySubMenu;
namespace vgui2
{
class Button;
class Label;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the class menu
//-----------------------------------------------------------------------------
class CBuyMenu : public vgui2::WizardPanel
{
public:
    void RefreshLanguage();
	CBuyMenu( vgui2::Panel* parent );
	~CBuyMenu() override;
	void ActivateBuyMenu();
	void ActivateEquipmentMenu();
	void UpdateBuyPresets( bool force );
	void OnClose() override;
	void Paint() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	bool IsFormattedForCareer() { return m_isCareer; }
	void Activate() override;

protected:
    void GetClientArea(int& x, int& y, int& width, int& height) override;
    void PerformLayout() override;
    void PaintBackground() override;

private:
	bool m_isCareer;
	CBuySubMenu* _mainMenu;
	CBuySubMenu* _equipMenu;
	vgui2::Panel* m_pMainBackground;
	vgui2::Button* m_pBuyPresetButtons[4];
	vgui2::Label* m_pMoney;
	int m_lastMoney;
};

void FitToFullScreen(vgui2::EditablePanel* panel);
void FitToFullScreen(vgui2::EditablePanel* panel, bool restoreResources);

#endif
