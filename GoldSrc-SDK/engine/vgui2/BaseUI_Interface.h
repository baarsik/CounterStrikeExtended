#pragma once

#include "../IBaseUI.h"
#include "../engine_apiproxy.h"
#include "../IGameUI.h"
#include "../IGameConsole.h"
#include "../iclientvgui.h"

#include <cstdint>
#include "../../public/native_gameui_retirement.h"
#if INTPTR_MAX > INT32_MAX
#include <memory>
#include "../../vgui2/controls/PHandle.h"
class IHTMLChromeController;
#endif

class BaseUISurface;
class CEnginePanel;
class CStaticPanel;
class ICareerUI;
class IMouseControl;
class SystemWrapper;

namespace vgui2
{
class IInputInternal;
class ISurface;
}

class CBaseUI : public IBaseUI
{
public:
	CBaseUI( const CBaseUI& other );
	CBaseUI();
	~CBaseUI() override;

	void Initialize( CreateInterfaceFn* factories, int count ) override;
	void Start( cl_enginefuncs_s* engineFunctions, int interfaceVersion ) override;
	void Shutdown() override;
    bool TryShutdown();
    bool TryRetireGameUI();

	int Key_Event( int down, int keynum, const char* currentBinding ) override;
	void CallEngineSurfaceAppHandler( void* event, void* userData ) override;
	void Paint( int x, int y, int right, int bottom ) override;

	void HideGameUI() override;
	void ActivateGameUI() override;
	void HideConsole() override;
	void ShowConsole() override;

private:
	CreateInterfaceFn	m_FactoryList[6];
	int					m_iNumFactories;

	CSysModule*			m_hVGuiModule;
	CSysModule*			m_hStaticGameUIModule;
	CSysModule*			m_hChromeModule;

	bool				m_bHidingGameUI;
#if INTPTR_MAX > INT32_MAX
    std::unique_ptr<IHTMLChromeController> m_nativeChromeController;
    const cse::gameui_lifetime::RetirementV1* m_nativeRetirement = nullptr;
    bool m_nativeGameUIInitialized = false;
    bool m_nativeControlsInitialized = false;
    bool m_nativeShutdownBusy = false;
    bool m_nativeRetirementBusy = false;
    bool m_nativeClientInitialized = false;
    bool m_nativeClientShutdown = false;
    bool m_nativeRootDeletionQueued = false;
    vgui2::DHANDLE<CStaticPanel> m_nativeRoot;
#endif
};
