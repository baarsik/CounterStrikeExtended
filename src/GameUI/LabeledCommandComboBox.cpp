//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LabeledCommandComboBox.h"
#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\LabeledCommandComboBox.cpp" )

CLabeledCommandComboBox::CLabeledCommandComboBox( vgui2::Panel* parent, const char* panelName )
	: BaseClass( parent, panelName, 6, false )
{
	AddActionSignalTarget( this );
	m_iCurrentSelection = -1;
	m_iStartSelection = -1;
}

CLabeledCommandComboBox::~CLabeledCommandComboBox()
{
}

void CLabeledCommandComboBox::DeleteAllItems() {
    BaseClass::DeleteAllItems();
    m_Items.RemoveAll();
    m_iCurrentSelection = -1;
    m_iStartSelection = -1;
}

bool CLabeledCommandComboBox::HasItem( const char* text ) const
{
	for ( int i = 0; i < m_Items.Size(); i++ )
	{
		if ( !strcmp( text, m_Items[i].name ) )
			return true;
	}

	return false;
}

void CLabeledCommandComboBox::AddItem( const char* text, const char* engineCommand )
{
	COMMANDITEM& item = m_Items[m_Items.AddToTail()];
	item.comboBoxID = BaseClass::AddItem( text, NULL );
	strcpy( item.name, text );

	strcpy( item.command, engineCommand );
}

void CLabeledCommandComboBox::ActivateItem(int index) {
    if (!m_Items.IsValidIndex(index)) {
        return;
    }
    BaseClass::ActivateItem(m_Items[index].comboBoxID);
    m_iCurrentSelection = index;
}

void CLabeledCommandComboBox::SetInitialItem(int index) {
    if (!m_Items.IsValidIndex(index)) {
        return;
    }
    m_iStartSelection = index;
    ActivateItem(index);
}

void CLabeledCommandComboBox::OnTextChanged( const char* text )
{
	for ( int i = 0; i < m_Items.Size(); i++ )
	{
		if (BaseClass::GetActiveItem() == m_Items[i].comboBoxID)
		{
			//	engine->pfnClientCmd( item->command );
			m_iCurrentSelection = i;
			break;
		}
	}

	if ( HasBeenModified() )
		PostActionSignal( new KeyValues( "ControlModified" ) );
//	PostMessage( GetParent()->GetVPanel(), new vgui::KeyValues( "TextChanged", "text", text ) );
}

const char* CLabeledCommandComboBox::GetActiveItemCommand() {
    if (!m_Items.IsValidIndex(m_iCurrentSelection)) {
        return nullptr;
    }
    return m_Items[m_iCurrentSelection].command;
}

void CLabeledCommandComboBox::ApplyChanges()
{
	if ( m_iCurrentSelection == -1 || m_Items.Size() <= 0 )
		return;

	ASSERT_AT( m_iCurrentSelection < m_Items.Size(), 124 );
	engine->pfnClientCmd( m_Items[m_iCurrentSelection].command );
	m_iStartSelection = m_iCurrentSelection;
}

bool CLabeledCommandComboBox::HasBeenModified()
{
	return m_iStartSelection != m_iCurrentSelection;
}

void CLabeledCommandComboBox::Reset()
{
	if ( m_iStartSelection != -1 )
		ActivateItem( m_iStartSelection );
}
