#include "../public/vgui1_style.h"
#if defined(__APPLE__)
#include "VGui1NativeRuntime.h"
#include "filesystem.h"
#include <cstdio>
#endif
#include "engine_apiproxy.h"
#include "native_gameui_retirement.h"
#include "IEngineSurface.h"
#include "VGUI_EngineSurfaceWrap.h"
#include "interface.h"
#include "ivideomode.h"
#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include <SDL3/SDL.h>

#include <VGUI_App.h>
#include <VGUI_Panel.h>
#include <VGUI_Scheme.h>

#undef null

class CDummyApp : public vgui::App
{
public:
	void main( int argc, char* argv[] ) override;

protected:
	void platTick() override;
};

CDummyApp theApp;

static vgui::Panel* staticPanel;
static EngineSurfaceWrap* staticEngineSurface;

SDL_Window** pmainwindow;

extern "C" void SetMouseEnable( qboolean fEnable );

void* VGuiWrap_GetPanel()
{
	g_engdstAddrs.VGui_GetPanel();
	return staticPanel;
}

void CDummyApp::main( int, char*[] )
{
}

void CDummyApp::platTick()
{
}

void VGuiWrap_SetRootPanelSize() {
    int x = 0, y = 0;
    int wide, tall;

    g_engdstAddrs.VGui_GetPanel();
    if (!staticPanel) {
        return;
    }

    SDL_GetWindowPosition(*pmainwindow, &x, &y);
    if (VideoMode_IsWindowed()) {
        SDL_GetWindowSizeInPixels(*pmainwindow, &wide, &tall);
    } else {
        VideoMode_GetCurrentVideoMode(&wide, &tall, NULL);
    }

    staticPanel->setBounds(x, y, wide, tall);
}

void VGuiWrap_Shutdown()
{
    cse::TryShutdownNativeVGuiWrap1();
}

#if defined(__APPLE__)
namespace {
cse_vgui1_native::Result nativeVGui1RetirementResult = cse_vgui1_native::Result::Ready;
}
cse_vgui1_native::Result cse::NativeVGui1RetirementResult() noexcept {
    return nativeVGui1RetirementResult;
}
#endif

bool cse::TryShutdownNativeVGuiWrap1() {
#if defined(__APPLE__)
    const auto result = cse_vgui1_native::DetachEmbedded(staticEngineSurface);
    if (result != cse_vgui1_native::Result::Ready) {
        if (result != nativeVGui1RetirementResult) {
            std::fprintf(stderr, "Native VGUI1 retirement refused (%d); dependent owners remain live\n",
                static_cast<int>(result));
        }
        nativeVGui1RetirementResult = result;
        return false;
    }
    nativeVGui1RetirementResult = result;
#endif
	::operator delete( staticPanel );
	staticPanel = NULL;

	delete staticEngineSurface;
	staticEngineSurface = NULL;
    CSE_VGui1BindStyle(nullptr);
    return true;
}

int VGuiWrap_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if ( staticEngineSurface )
		staticEngineSurface->AppHandler( event, userData );
	return 0;
}

void VGuiWrap_ReleaseMouse()
{
	if ( !vgui::App::getInstance() || !staticEngineSurface )
		return;

	if ( VGuiWrap2_UseVGUI1() )
	{
		staticEngineSurface->setCursor( vgui::App::getInstance()->getScheme()->getCursor( vgui::Scheme::scu_arrow ) );
		staticEngineSurface->lockCursor();
	}
	ClientDLL_DeactivateMouse();
	SetMouseEnable( FALSE );
}

void VGuiWrap_GetMouse()
{
	if ( !staticEngineSurface )
		return;

	if ( VGuiWrap2_UseVGUI1() )
		staticEngineSurface->unlockCursor();
	ClientDLL_ActivateMouse();
	SetMouseEnable( TRUE );
}

void VGuiWrap_SetVisible( int state )
{
	if ( staticPanel )
		staticPanel->setVisible( state != 0 );
}

void VGuiWrap_Paint( int paintAll )
{
	int extents[4];

	g_engdstAddrs.VGui_GetPanel();
	if ( !staticPanel )
		return;

#if defined(__APPLE__)
    cse_vgui1_native::Scope nativeUse(cse_vgui1_native::EmbeddedRuntime());
    if (nativeUse.Status() != cse_vgui1_native::Result::Ready) {
        cse_vgui1_native::InvalidEmbeddedUse("layout/paint before checked root admission");
    }
#endif
	VGuiWrap_SetRootPanelSize();
	staticPanel->repaint();
	vgui::App::getInstance()->externalTick();

	if ( paintAll )
	{
		staticPanel->paintTraverse();
	}
	else
	{
		staticPanel->getAbsExtents( extents[0], extents[1], extents[2], extents[3] );
		VGui_ViewportPaintBackground( extents );
	}
}

void VGuiWrap_Startup()
{
	CreateInterfaceFn factory;

	if ( staticEngineSurface )
		return;

#if defined(__APPLE__)
    constexpr std::size_t MaximumResourcePathBytes = 4096;
    char symbolFont[MaximumResourcePathBytes]{};
    if (!FS_GetLocalPath("resource/marlett.ttf", symbolFont, sizeof(symbolFont))) {
        cse_vgui1_native::InvalidEmbeddedUse("required game symbol font resource/marlett.ttf is missing");
    }
    const char* files[]{symbolFont};
    char diagnostic[512]{};
    const auto bound = cse_vgui1_native::BindEmbeddedFiles(files, 1, diagnostic, sizeof(diagnostic));
    if (bound != cse_vgui1_native::Result::Ready) {
        std::fprintf(stderr, "Native VGUI1 font binding failed: %s\n", diagnostic);
        cse_vgui1_native::InvalidEmbeddedUse("game font binding failed before layout");
    }
    cse_vgui1_native::Scope nativeUse(cse_vgui1_native::EmbeddedRuntime());
    if (nativeUse.Status() != cse_vgui1_native::Result::Ready) {
        cse_vgui1_native::InvalidEmbeddedUse("layout/paint before checked root admission");
    }
#endif
    CSE_VGui1BindStyle(cse::ui::QueryStyle(Sys_GetFactoryThis()));
	vgui::App::getInstance()->reset();
#if defined(__APPLE__)
    cse_vgui1_selection::SchemeReport report{};
    const auto prepared = cse_vgui1_native::PrepareEmbeddedScheme(vgui::App::getInstance()->getScheme(), report);
    if (prepared != cse_vgui1_selection::Result::Ready) {
        std::fprintf(stderr, "Native VGUI1 default font preparation failed: slot %u, result %d, service %d\n",
            report.failedSlot, static_cast<int>(prepared), report.serviceResult);
        cse_vgui1_native::InvalidEmbeddedUse("default scheme fonts are unavailable before layout");
    }
#endif


	staticPanel = new vgui::Panel( 0, 0, 320, 240 );
	staticPanel->setPaintBorderEnabled( false );
	staticPanel->setPaintBackgroundEnabled( false );
	staticPanel->setPaintEnabled( false );
	staticPanel->setCursor( vgui::App::getInstance()->getScheme()->getCursor( vgui::Scheme::scu_none ) );

	factory = Sys_GetFactoryThis();
	staticEngineSurface = new EngineSurfaceWrap( staticPanel, (IEngineSurface*)factory( "EngineSurface007", NULL ) );
	VGuiWrap_SetRootPanelSize();
}
