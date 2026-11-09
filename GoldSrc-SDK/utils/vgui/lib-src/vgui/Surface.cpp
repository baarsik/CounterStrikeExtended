#include "VGUI_Surface.h"

#include "VGUI_Panel.h"

using namespace vgui;

Surface::Surface( Panel* embeddedPanel ) : SurfaceBase( embeddedPanel )
{
	createPlat();
	recreateContext();
}

void Surface::createPopup( Panel* embeddedPanel )
{
	embeddedPanel->setParent( NULL );
	new Surface( embeddedPanel );
}

#if !defined(__APPLE__)
void Surface::GetMousePos( int&, int& )
{
}
#endif
