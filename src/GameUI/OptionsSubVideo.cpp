#include "OptionsSubVideo.h"
#include "CvarSlider.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"

#include "CheckButton.h"
#include "ComboBox.h"
#include "igameuifuncs.h"
#include "modes.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo( vgui2::Panel* parent ) : BaseClass( parent, NULL, true )
{
	memset( &m_OrigSettings, 0, sizeof( m_OrigSettings ) );
	memset( &m_CurrentSettings, 0, sizeof( m_CurrentSettings ) );

	m_pBrightnessSlider = new CCvarSlider( this, "Brightness", "#GameUI_Brightness", 0.0f, 2.0f, "brightness", false );
	m_pGammaSlider = new CCvarSlider( this, "Gamma", "#GameUI_Gamma", 1.0f, 3.0f, "gamma", false );
	GetVidSettings();

	m_pMode = new vgui2::ComboBox( this, "Resolution", 6, false );
	m_pAspectRatio = new vgui2::ComboBox( this, "AspectRatio", 2, false );
	m_pVsync = new CCvarToggleCheckButton( this, "Vsync", "#GameUI_VSync", "gl_vsync" );

	int normal = m_pAspectRatio->AddItem("#GameUI_AspectNormal", NULL);
	int widescreen = m_pAspectRatio->AddItem("#GameUI_AspectWide", NULL);
	m_bStartWidescreen = 3 * m_CurrentSettings.w != 4 * m_CurrentSettings.h;
	m_pAspectRatio->ActivateItem( m_bStartWidescreen ? widescreen : normal );

#if defined(__APPLE__) && defined(CSE_ENABLE_METAL)
    constexpr int kRendererRows = 3;
#else
    constexpr int kRendererRows = 2;
#endif
    m_pRenderer = new vgui2::ComboBox(this, "Renderer", kRendererRows, false);
    m_pRenderer->AddItem("#GameUI_Software", nullptr);
    m_pRenderer->AddItem("#GameUI_OpenGL", nullptr);
#if defined(__APPLE__) && defined(CSE_ENABLE_METAL)
    m_pRenderer->AddItem("Metal", nullptr);
#endif
    SetCurrentRendererComboItem();

	m_pWindowed = new vgui2::CheckButton( this, "Windowed", "#GameUI_Windowed" );
	m_pWindowed->SetSelected( m_CurrentSettings.windowed != 0 );
	m_pWidescreenFOV = new CCvarToggleCheckButton( this, "WidescreenFOV", "#GameUI_WidescreenFOV", "gl_widescreen_yfov" );
	m_pWidescreenFOV->SetEnabled( false );
	m_pWidescreenFOV->SetVisible( false );
	m_pTextureFiltering = new CCvarToggleCheckButton( this, "TextureFiltering", "#GameUI_TextureFiltering",
		"r_prefertexturefiltering" );
	m_pUseShaders = new CCvarToggleCheckButton( this, "UseShaders", "#GameUI_UseShaders", "gl_use_shaders" );

	LoadControlSettings( "Resource\\OptionsSubVideo.res", NULL );
	PrepareResolutionList( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
	m_CurrentSettings = m_OrigSettings;
	// reset UI elements
	m_pBrightnessSlider->Reset();
	m_pGammaSlider->Reset();
	m_pWindowed->SetSelected( m_CurrentSettings.windowed != 0 );
	m_pTextureFiltering->Reset();
	m_pVsync->Reset();
	m_pUseShaders->Reset();
	SetCurrentRendererComboItem();
	SetCurrentResolutionComboItem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentRendererComboItem() {
    m_iStartRenderer = kOpenGLRendererRow;
    if (!stricmp(m_CurrentSettings.renderer, "software")) {
        m_iStartRenderer = kSoftwareRendererRow;
    }
#if defined(__APPLE__) && defined(CSE_ENABLE_METAL)
    else if (!stricmp(m_CurrentSettings.renderer, "metal")) {
        m_iStartRenderer = kMetalRendererRow;
    }
#endif
    m_pRenderer->ActivateItemByRow(m_iStartRenderer);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
	vmode_s* plist = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &plist, &count );

	int resolution = -1;
	for ( int i = 0; i < count; i++, plist++ )
	{
		if ( plist->width == m_CurrentSettings.w && plist->height == m_CurrentSettings.h )
		{
			resolution = i;
			break;
		}
	}

	if ( resolution != -1 )
	{
		char sz[256];
		sprintf( sz, "%i x %i", plist->width, plist->height );
		m_pMode->SetText( sz );
	}
}

void COptionsSubVideo::PrepareResolutionList( bool disallowBadResolutions )
{
    constexpr int MinimumWidth = 640;
    constexpr int MinimumHeight = 480;
#if defined(_WIN32)
    constexpr int MaximumSoftwareWidth = 1599;
    constexpr int MaximumSoftwareHeight = 1199;
#else
    constexpr int MaximumSoftwareWidth = cse_native_software::MaximumWidth;
    constexpr int MaximumSoftwareHeight = cse_native_software::MaximumHeight;
#endif
	vmode_s* modes = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &modes, &count );

	m_pMode->DeleteAllItems();
	char text[256];
	m_pRenderer->GetText( text, sizeof( text ) );
    const bool software = m_pRenderer->GetActiveItem() == kSoftwareRendererRow;
	int selectedItem = -1;
	int itemsAdded = 0;
	bool foundWidescreen = false;
	for ( int i = 0; i < count; i++ )
	{
		vmode_s& mode = modes[i];
		sprintf( text, "%i x %i", mode.width, mode.height );
        if (disallowBadResolutions && count > 1) {
            bool belowMinimum = mode.width < MinimumWidth || mode.height < MinimumHeight;
#if defined(__APPLE__)
            if (software && m_iStartRenderer == kSoftwareRendererRow &&
                mode.width == m_OrigSettings.w && mode.height == m_OrigSettings.h) {
                belowMinimum = false;
            }
#endif
            if (belowMinimum) {
                continue;
            }
            if (software && (mode.width > MaximumSoftwareWidth || mode.height > MaximumSoftwareHeight)) {
                continue;
            }
        }

		int item = -1;
		if ( 3 * mode.width == 4 * mode.height )
		{
			if ( !m_bStartWidescreen )
			{
				item = m_pMode->AddItem( text, NULL );
				itemsAdded++;
			}
		}
		else
		{
			if ( m_bStartWidescreen )
			{
				item = m_pMode->AddItem( text, NULL );
				itemsAdded++;
			}
			foundWidescreen = true;
		}

		if ( mode.width == m_CurrentSettings.w && mode.height == m_CurrentSettings.h )
			selectedItem = item;
	}

	m_pAspectRatio->SetEnabled( foundWidescreen );
	m_pMode->ActivateItem( selectedItem == -1 ? 0 : selectedItem );
	if ( !itemsAdded && count != 0 )
	{
		m_bStartWidescreen = !m_bStartWidescreen;
		m_pAspectRatio->ActivateItem( ( m_pAspectRatio->GetActiveItem() + 1 ) % 2 );
		PrepareResolutionList( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnButtonChecked(KeyValues* data) {
    const int state = data->GetInt("state", 0);
    auto* panel = static_cast<vgui2::Panel*>(data->GetPtr("panel", nullptr));
    if (panel == m_pWindowed) {
        if (state != m_CurrentSettings.windowed) {
            OnDataChanged();
        }
    } else if (panel == m_pVsync) {
        // Reset notifications describe the saved value, not an edit.
        if (m_pVsync->HasBeenModified()) {
            OnDataChanged();
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged( vgui2::Panel* panel, const char* text )
{
	if ( panel == m_pMode )
	{
		char resolution[256];
		sprintf( resolution, "%i x %i", m_CurrentSettings.w, m_CurrentSettings.h );
		if ( strcmp( text, resolution ) != 0 )
			OnDataChanged();
	}
	else if ( panel == m_pRenderer )
	{
		PrepareResolutionList( true );
		if (m_pRenderer->GetActiveItem() != m_iStartRenderer)
			OnDataChanged();
	}
	else if ( panel == m_pAspectRatio )
	{
		if (m_pAspectRatio->GetActiveItem() != static_cast<int>(m_bStartWidescreen))
		{
			m_bStartWidescreen = !m_bStartWidescreen;
			PrepareResolutionList( true );
		}
	}
}

void COptionsSubVideo::GetVidSettings()
{
	gameuifuncs->GetCurrentVideoMode( &m_OrigSettings.w, &m_OrigSettings.h, &m_OrigSettings.bpp );
	int hdModels, addons;
	gameuifuncs->GetCurrentRenderer( m_OrigSettings.renderer, sizeof( m_OrigSettings.renderer ),
		&m_OrigSettings.windowed, &hdModels, &addons );
#ifdef _WIN32
	strlwr( m_OrigSettings.renderer );
#endif
	m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::RevertVidSettings()
{
	m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::ApplyVidSettings( bool forceRefresh )
{
	char text[256];
	if ( m_pMode )
	{
		m_pMode->GetText( text, sizeof( text ) );
		int w = 0, h = 0;
		if ( sscanf( text, "%i x %i", &w, &h ) == 2 )
		{
			m_CurrentSettings.w = w;
			m_CurrentSettings.h = h;
			m_CurrentSettings.bpp = 32;
		}
	}
	if ( m_pRenderer )
	{
		m_pRenderer->GetText( text, sizeof( text ) );
        const int rendererRow = m_pRenderer->GetActiveItem();
        if (rendererRow == kSoftwareRendererRow) {
            strcpy(m_CurrentSettings.renderer, "software");
        } else if (rendererRow == kOpenGLRendererRow) {
            strcpy(m_CurrentSettings.renderer, "gl");
        }
#if defined(__APPLE__) && defined(CSE_ENABLE_METAL)
        else if (rendererRow == kMetalRendererRow) {
            strcpy(m_CurrentSettings.renderer, "metal");
        }
#endif

		if ( stricmp( m_CurrentSettings.renderer, m_OrigSettings.renderer ) == 0 && m_OrigSettings.bpp == 16 )
			m_CurrentSettings.bpp = 16;
	}
	if ( m_pWindowed )
		m_CurrentSettings.windowed = m_pWindowed->IsSelected();
	if ( m_pVsync )
		m_pVsync->ApplyChanges();

	if ( memcmp( &m_OrigSettings, &m_CurrentSettings, sizeof( m_OrigSettings ) ) != 0 || forceRefresh )
	{
		sprintf( text, "_setvideomode %i %i %i\n", m_CurrentSettings.w, m_CurrentSettings.h, m_CurrentSettings.bpp );
		engine->pfnClientCmd( text );
		sprintf( text, "_setrenderer %s %s\n", m_CurrentSettings.renderer,
			m_CurrentSettings.windowed ? "windowed" : "fullscreen" );
		engine->pfnClientCmd( text );
		engine->pfnClientCmd( (char*)"_restart\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{
	bool changed = m_pGammaSlider->HasBeenModified();
	m_pBrightnessSlider->ApplyChanges();
	m_pGammaSlider->ApplyChanges();
	m_pWidescreenFOV->ApplyChanges();
	m_pTextureFiltering->ApplyChanges();
	m_pUseShaders->ApplyChanges();
	ApplyVidSettings( changed );
}
