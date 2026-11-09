#if defined(__APPLE__)
#include "VGui1NativeRuntime.h"
#endif
#include "../common/sdl3_input.h"
#include "../common/sdl3_cursor.h"
#include "VGUI_EngineSurfaceWrap.h"

#include <VGUI_App.h>
#include <VGUI_Cursor.h>
#include <VGUI_BitmapTGA.h>
#include <VGUI_FileInputStream.h>
#include <VGUI_Font.h>
#include <VGUI_Frame.h>
#include <VGUI_Panel.h>
#include <VGUI_ImagePanel.h>

#undef null
#undef VGUIAPI

#include "IEngineSurface.h"
#include "cvar.h"
#include "igame.h"
#include "ivideomode.h"
#include "vid.h"
#include "vgui2/imousecontrol.h"
#include "vgui_intwrap2.h"
#include "../vgui2/vgui_surfacelib/vgui_key_translation.h"
#include <SDL3/SDL.h>
#include <cctype>

#include <cstddef>
#include <cstring>
#include <new>
#include "sdl3_window.h"

#if !defined(__APPLE__)
class FontInfoVGUI
{
public:
	int id;
	int pageCount;
	int pageForChar[256];
	int bindIndex[8];
	float texCoord[256][8];
	int contextCount;
};
#endif

namespace
{
#if !defined(__APPLE__)
constexpr int FONT_TEXTURE_SIZE = 512;
constexpr int FONT_TEXTURE_BYTES = FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE * 4;
constexpr float FONT_TEXTURE_COORD_SCALE = 0.001953125f;
constexpr int FONT_PAGE_LIMIT = 8;
#endif
constexpr int CURSOR_SLOT_COUNT = 20;

#if !defined(__APPLE__)
static_assert( sizeof( FontInfoVGUI ) == 0x242C, "FontInfoVGUI" );
#endif
static_assert( vgui::KEY_0 == 0, "vgui key" );
static_assert( vgui::KEY_CAPSLOCK == 67, "vgui key" );
static_assert( vgui::KEY_LSHIFT == 78, "vgui key" );
static_assert( vgui::KEY_LALT == 80, "vgui key" );
static_assert( vgui::KEY_RALT == 81, "vgui key" );
static_assert( vgui::KEY_LCONTROL == 82, "vgui key" );
static_assert( vgui::KEY_RCONTROL == 83, "vgui key" );
static_assert( vgui::KEY_LWIN == 84, "vgui key" );
static_assert( vgui::KEY_RWIN == 85, "vgui key" );
static_assert( vgui::MOUSE_LEFT == 0, "vgui mouse" );
static_assert( vgui::MOUSE_RIGHT == 1, "vgui mouse" );
static_assert( vgui::MOUSE_MIDDLE == 2, "vgui mouse" );
static_assert( vgui::Cursor::dc_user == 0, "vgui cursor" );
static_assert( vgui::Cursor::dc_none == 1, "vgui cursor" );
static_assert( vgui::Cursor::dc_arrow == 2, "vgui cursor" );
static_assert( vgui::Cursor::dc_hand == 13, "vgui cursor" );
static_assert(SDL_KMOD_LSHIFT == 0x0001, "keymod");
static_assert(SDL_KMOD_RSHIFT == 0x0002, "keymod");
static_assert(SDL_KMOD_LCTRL == 0x0040, "keymod");
static_assert(SDL_KMOD_RCTRL == 0x0080, "keymod");
static_assert(SDL_KMOD_LALT == 0x0100, "keymod");
static_assert(SDL_KMOD_RALT == 0x0200, "keymod");
static_assert(SDL_KMOD_LGUI == 0x0400, "keymod");
static_assert(SDL_KMOD_RGUI == 0x0800, "keymod");
static_assert(SDL_KMOD_CAPS == 0x2000, "keymod");

#if !defined(__APPLE__)
static vgui::Font* staticFont = nullptr;
static FontInfoVGUI* staticFontInfoVGUI = nullptr;
static vgui::Dar<FontInfoVGUI*> staticFontInfoVGUIDar;
static int staticContextCount = 0;
static unsigned char staticRGBA[FONT_TEXTURE_BYTES];
#endif
static cse::FractionalMotion s_wheelMotion;
static SDL_Cursor* staticDefaultCursor[CURSOR_SLOT_COUNT];
static SDL_Cursor* staticCurrentCursor = nullptr;

static void CheckModState(vgui::App *app, vgui::SurfaceBase *surface) {
    static int s_lastModifierCode = 0;
    const int changed = s_lastModifierCode ^ static_cast<int>(SDL_GetModState());

    auto report = [&](int mask, vgui::KeyCode code) {
        if ((changed & mask) == 0) {
            return;
        }

        if ((s_lastModifierCode & mask) != 0) {
            app->internalKeyReleased(code, surface);
        } else {
            app->internalKeyPressed(code, surface);
        }
    };

    report(SDL_KMOD_LSHIFT, vgui::KEY_LSHIFT);
    report(SDL_KMOD_LALT, vgui::KEY_LALT);
    report(SDL_KMOD_LCTRL, vgui::KEY_LCONTROL);
    report(SDL_KMOD_RSHIFT, vgui::KEY_LSHIFT);
    report(SDL_KMOD_RALT, vgui::KEY_RALT);
    report(SDL_KMOD_RCTRL, vgui::KEY_RCONTROL);
    report(SDL_KMOD_CAPS, vgui::KEY_CAPSLOCK);
    report(SDL_KMOD_LGUI, vgui::KEY_LWIN);
    report(SDL_KMOD_RGUI, vgui::KEY_RWIN);
    s_lastModifierCode = static_cast<int>(SDL_GetModState());
}
}

bool s_bCursorVisible = false;
static bool s_bUsedRawInputLast = false;
extern IMouseControl* mousecontrol;
extern cvar_t m_rawinput;
extern qboolean BUsesSDLInput();

EngineSurfaceWrap::EngineSurfaceWrap(vgui::Panel *embeddedPanel, IEngineSurface *engineSurface)
    : vgui::SurfaceBase(embeddedPanel), _mouseBitmap(nullptr), _engineSurface(engineSurface), _cursorLocked(false) {
#if defined(__APPLE__)
    if (cse_vgui1_native::AttachEmbedded(this, engineSurface) != cse_vgui1_native::Result::Ready) {
        cse_vgui1_native::InvalidEmbeddedUse("surface construction before checked bootstrap admission");
    }
#else
    staticFont = nullptr;
    staticFontInfoVGUI = nullptr;
    staticFontInfoVGUIDar.setCount(0);
    ++staticContextCount;
#endif

    vgui::FileInputStream *stream = new vgui::FileInputStream("valve/gfx/vgui/mouse.tga", false);
    _mouseBitmap = new vgui::BitmapTGA(stream, true);
    stream->close();
    ::operator delete(stream);

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
#if !defined(_WIN32)
        SDL_SetHint("SDL_VIDEO_X11_XRANDR", "1");
        SDL_SetHint("SDL_VIDEO_X11_XVIDMODE", "1");
#endif
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    }

    staticDefaultCursor[vgui::Cursor::dc_none] = nullptr;
    staticDefaultCursor[vgui::Cursor::dc_arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    staticDefaultCursor[vgui::Cursor::dc_ibeam] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    staticDefaultCursor[vgui::Cursor::dc_hourglass] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
    staticDefaultCursor[vgui::Cursor::dc_crosshair] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    staticDefaultCursor[vgui::Cursor::dc_up] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    staticDefaultCursor[vgui::Cursor::dc_sizenwse] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
    staticDefaultCursor[vgui::Cursor::dc_sizenesw] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
    staticDefaultCursor[vgui::Cursor::dc_sizewe] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    staticDefaultCursor[vgui::Cursor::dc_sizens] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    staticDefaultCursor[vgui::Cursor::dc_sizeall] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    staticDefaultCursor[vgui::Cursor::dc_no] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
    staticDefaultCursor[vgui::Cursor::dc_hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    staticCurrentCursor = staticDefaultCursor[vgui::Cursor::dc_arrow];
}

EngineSurfaceWrap::~EngineSurfaceWrap() {
    ::operator delete(_mouseBitmap);
    for (int index = 0; index < CURSOR_SLOT_COUNT; ++index) {
        if (staticDefaultCursor[index] != nullptr) {
            SDL_DestroyCursor(staticDefaultCursor[index]);
        }
    }
}

void EngineSurfaceWrap::setTitle( const char* )
{
}

bool EngineSurfaceWrap::setFullscreenMode( int, int, int )
{
	return false;
}

void EngineSurfaceWrap::setWindowedMode()
{
}

void EngineSurfaceWrap::setAsTopMost( bool )
{
}

void EngineSurfaceWrap::createPopup( vgui::Panel* )
{
}

bool EngineSurfaceWrap::hasFocus()
{
	return true;
}

bool EngineSurfaceWrap::isWithin( int, int )
{
	return true;
}

int EngineSurfaceWrap::createNewTextureID()
{
	return _engineSurface->createNewTextureID();
}

void EngineSurfaceWrap::GetMousePos(int &x, int &y) {
    cse::GetEngineMousePosition((SDL_Window *)game->GetMainWindow(), &x, &y);
    if (VideoMode_IsWindowed() == 0) {
        return;
    }

    int windowX = 0;
    int windowY = 0;
    SDL_GetWindowPosition(reinterpret_cast<SDL_Window *>(game->GetMainWindow()), &windowX, &windowY);
    x += windowX;
    y += windowY;
}

void EngineSurfaceWrap::drawSetColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetColor( r, g, b, a );
}

void EngineSurfaceWrap::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawFilledRect( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawOutlinedRect( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::drawSetTextFont( vgui::Font* font )
{
#if defined(__APPLE__)
    const auto result = cse_vgui1_native::SelectEmbedded(this, font);
    if (result != cse_vgui1_native::Result::Ready) {
        cse_vgui1_native::InvalidEmbeddedUse("checked native font/atlas use failed");
    }
#else
	staticFont = font;
	if ( font == nullptr )
		return;

	staticFontInfoVGUI = nullptr;
	bool rebuild = false;
	for ( int index = 0; index < staticFontInfoVGUIDar.getCount(); ++index )
	{
		FontInfoVGUI* info = staticFontInfoVGUIDar[index];
		if ( info->id != staticFont->getId() )
			continue;

		staticFontInfoVGUI = staticFontInfoVGUIDar[index];
		if ( staticFontInfoVGUI->contextCount != staticContextCount )
		{
			rebuild = true;
			break;
		}
	}

	if ( staticFontInfoVGUI )
	{
		if ( !rebuild )
			return;
	}
	else
	{
		FontInfoVGUI* created = static_cast<FontInfoVGUI*>( ::operator new( sizeof( FontInfoVGUI ) ) );
		created->id = 0;
		created->pageCount = 0;
		created->bindIndex[0] = 0;
		created->bindIndex[1] = 0;
		created->bindIndex[2] = 0;
		created->bindIndex[3] = 0;
		std::memset( created->pageForChar, 0, sizeof( created->pageForChar ) );
		created->contextCount = -1;
		staticFontInfoVGUI = created;
		created->id = staticFont->getId();
		staticFontInfoVGUIDar.addElement( created );
	}

	staticFontInfoVGUI->contextCount = staticContextCount;
	std::memset( staticRGBA, 0, sizeof( staticRGBA ) );

	int page = 0;
	int penX = 0;
	int penY = 0;
	for ( int character = 0; character < 256; )
	{
		int abcA = 0;
		int abcB = 0;
		int abcC = 0;
		staticFont->getCharABCwide( character, abcA, abcB, abcC );
#if defined( _WIN32 ) || UINTPTR_MAX > UINT32_MAX
		const int wide = abcB;
#else
		const int wide = staticFont->getWide();
#endif
#if !defined( _WIN32 ) && UINTPTR_MAX <= UINT32_MAX
		if ( std::isprint( character ) == 0 )
		{
			++character;
			continue;
		}
#endif

		const int tall = staticFont->getTall();
		if ( wide + penX > 511 )
		{
			penX = 0;
			penY += tall + 1;
		}

		if ( tall + penY > 511 )
		{
			int texture = staticFontInfoVGUI->bindIndex[page];
			if ( texture == 0 )
			{
				texture = createNewTextureID();
				staticFontInfoVGUI->bindIndex[page] = texture;
			}
			drawSetTextureRGBA(
				texture, reinterpret_cast<const char*>( staticRGBA ), FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE );
			++page;
			if ( page == FONT_PAGE_LIMIT )
				break;

			std::memset( staticRGBA, 0, sizeof( staticRGBA ) );
			penX = 0;
			penY = 0;
		}

		staticFont->getCharRGBA( character, penX, penY, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, staticRGBA );
		const int xEnd = wide + penX;
		const int yEnd = tall + penY;
		staticFontInfoVGUI->pageForChar[character] = page;
		float* coord = staticFontInfoVGUI->texCoord[character];
		coord[0] = static_cast<float>( penX ) * FONT_TEXTURE_COORD_SCALE;
		coord[1] = static_cast<float>( penY ) * FONT_TEXTURE_COORD_SCALE;
		coord[2] = static_cast<float>( xEnd ) * FONT_TEXTURE_COORD_SCALE;
		coord[3] = static_cast<float>( yEnd ) * FONT_TEXTURE_COORD_SCALE;
		penX += wide + 1;
		++character;
	}

	int* textureSlot = reinterpret_cast<int*>(
		reinterpret_cast<unsigned char*>( staticFontInfoVGUI ) + offsetof( FontInfoVGUI, bindIndex ) + page * sizeof( int ) );
	int texture = *textureSlot;
	if ( texture == 0 )
	{
		texture = createNewTextureID();
		*textureSlot = texture;
	}
	drawSetTextureRGBA( texture, reinterpret_cast<const char*>( staticRGBA ), FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE );
	staticFontInfoVGUI->pageCount = page + 1;
#endif
}

void EngineSurfaceWrap::drawSetTextColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetTextColor( r, g, b, a );
}

void EngineSurfaceWrap::drawSetTextPos( int x, int y )
{
	_engineSurface->drawSetTextPos( x, y );
}

void EngineSurfaceWrap::drawPrintText( const char* text, int textLen )
{
#if defined(__APPLE__)
    const auto result = cse_vgui1_native::DrawEmbedded(this, text, textLen);
    if (result != cse_vgui1_native::Result::Ready) {
        cse_vgui1_native::InvalidEmbeddedUse("checked native font/atlas use failed");
    }
#else
	if ( text == nullptr || staticFont == nullptr || staticFontInfoVGUI == nullptr )
		return;

	int x = 0;
	int y = 0;
	drawGetTextPos( x, y );
	const int tall = staticFont->getTall();
	int penX = x;
	if ( textLen > 0 )
	{
		const char* cursor = text;
		const char* end = text + textLen;
		while ( cursor != end )
		{
			const int character = static_cast<unsigned char>( *cursor );
			int abcA = 0;
			int abcB = 0;
			int abcC = 0;
			staticFont->getCharABCwide( character, abcA, abcB, abcC );
			x += abcA;
#if defined( _WIN32 ) || UINTPTR_MAX > UINT32_MAX
			const int wide = abcB;
#else
			const int wide = staticFont->getWide();
#endif
			drawSetTexture( staticFontInfoVGUI->bindIndex[staticFontInfoVGUI->pageForChar[character]] );
			const float* coord = staticFontInfoVGUI->texCoord[character];
			drawPrintChar( x, y, wide, tall, coord[0], coord[1], coord[2], coord[3] );
			++cursor;
			penX = abcC + x + wide;
			x = penX;
		}
	}

	drawSetTextPos( penX, y );
#endif
}

void EngineSurfaceWrap::drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	_engineSurface->drawSetTextureRGBA( id, rgba, wide, tall, 0, 0 );
}

void EngineSurfaceWrap::drawSetTexture( int id )
{
	_engineSurface->drawSetTexture( id );
}

void EngineSurfaceWrap::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawTexturedRect( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::invalidate( vgui::Panel* )
{
}

void EngineSurfaceWrap::enableMouseCapture( bool )
{
}

void EngineSurfaceWrap::setCursor(vgui::Cursor *cursor) {
    if (mousecontrol->VGUI2MouseControl() || VGuiWrap2_IsGameUIVisible() != 0 || cursor == nullptr || _cursorLocked) {
        return;
    }

    const bool wasVisible = s_bCursorVisible;
    _currentCursor = cursor;
    s_bCursorVisible = true;
    _emulatedCursor->setImage(_mouseBitmap);

    const int cursorValue = cursor->getDefaultCursor();
    if (cursorValue == vgui::Cursor::dc_none) {
        _emulatedCursor->setImage(nullptr);
        s_bCursorVisible = false;
    }
#if defined(_WIN32)
    if (cursorValue >= vgui::Cursor::dc_none && cursorValue <= vgui::Cursor::dc_hand) {
        staticCurrentCursor = staticDefaultCursor[cursor->getDefaultCursor()];
    }
#else
    else if (cursorValue != vgui::Cursor::dc_user && cursorValue <= vgui::Cursor::dc_hand) {
        staticCurrentCursor = staticDefaultCursor[cursorValue];
    }
#endif

    if (s_bCursorVisible) {
        SDL_SetCursor(staticCurrentCursor);
    }

    const bool useRawInput = m_rawinput.value != 0.0f && BUsesSDLInput() != 0;
#if INTPTR_MAX > INT32_MAX
    cse::NativeMouseWindow &mouseWindow = cse::NativeMouseWindowState();
    if (cse::CursorRequestChanged(wasVisible, s_bCursorVisible, s_bUsedRawInputLast, useRawInput) ||
        mouseWindow.NeedsRetry(cse::NativeCursorOwner::VGui1)) {
        SDL_Window *window = reinterpret_cast<SDL_Window *>(game->GetMainWindow());
        cse::NativeCursorTransaction transaction;
        if (mouseWindow.Begin(window, cse::NativeCursorOwner::VGui1, s_bCursorVisible, useRawInput, transaction)) {
#if defined(_WIN32)
            if (wasVisible && !s_bCursorVisible && BUsesSDLInput() == 0) {
                int windowWidth = 0;
                int windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                SDL_WarpMouseInWindow(window, static_cast<float>(windowWidth / 2),
                                     static_cast<float>(windowHeight / 2));
            }
#endif
            if (!s_bCursorVisible && transaction.Current()) {
                SDL_PumpEvents();
                if (transaction.Current()) {
                    SDL_GetRelativeMouseState(nullptr, nullptr);
                }
            }
            if (transaction.Commit()) {
                s_bUsedRawInputLast = useRawInput;
            }
        }
    }
#else
    if (cse::CursorRequestChanged(wasVisible, s_bCursorVisible, s_bUsedRawInputLast, useRawInput)) {
        SDL_Window *window = reinterpret_cast<SDL_Window *>(game->GetMainWindow());
        cse::ApplyCursorVisibility(window, s_bCursorVisible, useRawInput);

#if defined(_WIN32)
        if (wasVisible && !s_bCursorVisible && BUsesSDLInput() == 0) {
            int windowWidth = 0;
            int windowHeight = 0;
            SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            SDL_WarpMouseInWindow(window, static_cast<float>(windowWidth / 2), static_cast<float>(windowHeight / 2));
        }
#endif

        if (!s_bCursorVisible) {
            SDL_PumpEvents();
            SDL_GetRelativeMouseState(nullptr, nullptr);
        }
    }
    s_bUsedRawInputLast = useRawInput;
#endif
}

void EngineSurfaceWrap::swapBuffers()
{
}

void EngineSurfaceWrap::pushMakeCurrent( vgui::Panel* panel, bool useInsets )
{
	int insets[4] = { 0, 0, 0, 0 };
	if ( useInsets )
		panel->getInset( insets[0], insets[1], insets[2], insets[3] );

	int absoluteExtents[4];
	int clipRect[4];
	panel->getAbsExtents( absoluteExtents[0], absoluteExtents[1], absoluteExtents[2], absoluteExtents[3] );
	panel->getClipRect( clipRect[0], clipRect[1], clipRect[2], clipRect[3] );
	_engineSurface->pushMakeCurrent( insets, absoluteExtents, clipRect, true );
}

void EngineSurfaceWrap::popMakeCurrent( vgui::Panel* )
{
	_engineSurface->popMakeCurrent();
}

void EngineSurfaceWrap::applyChanges()
{
}

void EngineSurfaceWrap::AppHandler(void *event, void *) {
    vgui::App *app = vgui::App::getInstance();
#if defined(_WIN32)
    if (this == nullptr || app == nullptr) {
#else
    // A valid member call has a non-null receiver; the app can be unavailable.
    if (app == nullptr) {
#endif
        return;
    }

    const SDL_Event *sdlEvent = static_cast<SDL_Event *>(event);
    const unsigned int type = sdlEvent->type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        int x = 0;
        int y = 0;
        cse::WindowToEngineMousePosition((SDL_Window *)game->GetMainWindow(), sdlEvent->motion.x, sdlEvent->motion.y, x,
                                         y);
        app->internalCursorMoved(x, y, this);
        return;
    }

    if (type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP || type == SDL_EVENT_MOUSE_WHEEL) {
        if (type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP) {
            const int button = sdlEvent->button.button;
            vgui::MouseCode code = vgui::MOUSE_LEFT;
            if (button == SDL_BUTTON_MIDDLE) {
                code = vgui::MOUSE_MIDDLE;
            } else if (button == SDL_BUTTON_RIGHT) {
                code = vgui::MOUSE_RIGHT;
            }

            if (type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                app->internalMousePressed(code, this);
            } else {
                app->internalMouseReleased(code, this);
            }
        } else if (type == SDL_EVENT_MOUSE_WHEEL) {
            app->internalMouseWheeled(cse::WheelSteps(s_wheelMotion, sdlEvent->wheel.y, sdlEvent->wheel.direction),
                                      this);
        }
        return;
    }

    if (type == SDL_EVENT_KEY_DOWN) {
        const vgui::KeyCode code = static_cast<vgui::KeyCode>(KeyCode_VirtualKeyToVGUI(sdlEvent->key.key) - 1);
        app->internalKeyPressed(code, this);
        app->internalKeyTyped(code, this);
        CheckModState(app, this);
        return;
    }

    if (type != SDL_EVENT_KEY_UP) {
        return;
    }

    const vgui::KeyCode code = static_cast<vgui::KeyCode>(KeyCode_VirtualKeyToVGUI(sdlEvent->key.key) - 1);
    app->internalKeyReleased(code, this);
    CheckModState(app, this);
}

void EngineSurfaceWrap::lockCursor()
{
	_cursorLocked = true;
}

void EngineSurfaceWrap::unlockCursor()
{
	_cursorLocked = false;
	setCursor( _currentCursor );
}

void EngineSurfaceWrap::drawLine( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawLine( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::drawPolyLine( int* x, int* y, int count )
{
	_engineSurface->drawPolyLine( x, y, count );
}

void EngineSurfaceWrap::drawTexturedPolygon( vgui::VGuiVertex* vertices, int count )
{
	_engineSurface->drawTexturedPolygon( vertices, count );
}

void EngineSurfaceWrap::drawSetTextureBGRA(
	int id, const char* rgba, int wide, int tall, int hardwareFilter, int forceReload )
{
	_engineSurface->drawSetTextureBGRA( id, rgba, wide, tall, hardwareFilter, forceReload );
}

void EngineSurfaceWrap::drawUpdateRegionTextureBGRA(
	int id, int x, int y, const unsigned char* data, int wide, int tall )
{
	_engineSurface->drawUpdateRegionTextureBGRA( id, x, y, data, wide, tall );
}

void EngineSurfaceWrap::drawGetTextPos( int& x, int& y )
{
	_engineSurface->drawGetTextPos( x, y );
}

void EngineSurfaceWrap::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintChar( x, y, wide, tall, s0, t0, s1, t1 );
}

void EngineSurfaceWrap::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintCharAdd( x, y, wide, tall, s0, t0, s1, t1 );
}
