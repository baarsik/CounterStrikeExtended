//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"
#include "PHandle.h"

class CBitmapImagePanel;
class CCvarSlider;
class CCvarTextEntry;
class CCvarToggleCheckButton;
class CLabeledCommandComboBox;
class CMultiplayerAdvancedDialog;
class CrosshairImagePanel;

namespace vgui2
{
class ComboBox;
class Label;
}

//-----------------------------------------------------------------------------
// Purpose: multiplayer options property page
//-----------------------------------------------------------------------------
class COptionsSubMultiplayer : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( COptionsSubMultiplayer, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	COptionsSubMultiplayer( vgui2::Panel* parent );
	~COptionsSubMultiplayer() override;

protected:
	void OnCommand( const char* command ) override;
	void OnResetData() override;
	void OnApplyChanges() override;
    void PerformLayout() override;
	void RedrawCrosshairImage();

private:
	void InitModelList( CLabeledCommandComboBox* combo );
	void InitLogoList( CLabeledCommandComboBox* combo );
	void RemapModel();
	void RemapLogo();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );
	MESSAGE_FUNC( OnApplyButtonEnable, "ControlModified" );

	void RemapPalette( char* filename, int topColor, int bottomColor );
	void RemapLogoPalette( char* filename, int r, int g, int b );
	void ColorForName( const char* name, int& r, int& g, int& b );
	void InitCrosshairColorEntries();
	void ApplyCrosshairColorChanges();

	CBitmapImagePanel*				m_pModelImage;
	CLabeledCommandComboBox*			m_pModelList;
	CBitmapImagePanel*				m_pLogoImage;
	CLabeledCommandComboBox*			m_pLogoList;
	char							m_LogoName[32];
    vgui2::ComboBox* m_pCrosshairSize = nullptr;
    vgui2::ComboBox* m_pCrosshairShape = nullptr;
    vgui2::Label* m_pCrosshairShapeLabel = nullptr;
	CCvarToggleCheckButton*			m_pCrosshairTranslucencyCheckbox;
	vgui2::ComboBox*					m_pCrosshairColorComboBox;
	int								m_displayedCrosshairColorRed;
	int								m_displayedCrosshairColorGreen;
	int								m_displayedCrosshairColorBlue;
	CLabeledCommandComboBox*			m_pColorList;
	CCvarTextEntry*					m_pNameTextEntry;
	CCvarSlider*					m_pPrimaryColorSlider;
	CCvarSlider*					m_pSecondaryColorSlider;
	CCvarToggleCheckButton*			m_pHighQualityModelCheckBox;
	CrosshairImagePanel*				m_pCrosshairImage;
	int								m_nTopColor;
	int								m_nBottomColor;
	int								m_nLogoR;
	int								m_nLogoG;
	int								m_nLogoB;
	vgui2::DHANDLE<CMultiplayerAdvancedDialog> m_hMultiplayerAdvancedDialog;
};
