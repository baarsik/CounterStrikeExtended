#if defined(__APPLE__)
#include "VGui1NativeRuntime.h"
#endif
#include "vgui_EngineSurface.h"
#include "draw.h"
#include "screen.h"
#include "engine_apiproxy.h"
#include "filesystem.h"
#include "client.h"
#include "keys.h"
#include "ivideomode.h"
#include "vgui_int.h"
#include "view.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

#define TEXTURE_HASH_SIZE	317

typedef struct vgui_texture_s
{
	struct vgui_texture_s*	next;
	int						id;
	int						wide;
	int						tall;
	unsigned char*			prgba;
} vgui_texture_t;

class EngineSurfaceTextures
{
public:
	EngineSurfaceTextures();
	~EngineSurfaceTextures();

	void InitTextures();
	void FreeTextures();
	int GetHashVal( int id );
	void Insert( int id, int wide, int tall, const char* pixels );
	void Remove( int id );
#if defined(__APPLE__)
    bool RetireAtlasPage(int id);
#endif
	vgui_texture_t* GetTexture( int id );

private:
	vgui_texture_t*	hash[TEXTURE_HASH_SIZE];
};

class EngineSurfaceRenderPlat
{
public:
	int						left;
	int						top;
	int						right;
	int						bottom;
	EngineSurfaceTextures*	textures;
	vgui_texture_t*			current;
};

int g_TranslateX;
int g_TranslateY;

extern SDL_Window** pmainwindow;

EngineSurface* EngineSurface::s_pEngineSurface = NULL;

EXPOSE_SINGLE_INTERFACE( EngineSurface, IEngineSurface, "EngineSurface007" );

EngineSurface::EngineSurface()
{
	_renderPlat = NULL;
	createRenderPlat();
#if defined(__APPLE__)
    cse_vgui1_native::RememberEngineSurface(this);
#endif
}

EngineSurface::~EngineSurface()
{
#if defined(__APPLE__)
    cse_vgui1_native::ForgetEngineSurface(this);
#endif
	deleteRenderPlat();
#ifdef _WIN32
	_renderPlat = NULL;
#endif
	s_pEngineSurface = NULL;
}

void EngineSurface::freeEngineSurface()
{
	if ( !s_pEngineSurface )
		return;

	delete s_pEngineSurface;
	s_pEngineSurface = NULL;
}

int EngineSurface::createNewTextureID()
{
	static int textNum = 4000;
	return ++textNum;
}

void EngineSurface::drawSetColor( int red, int green, int blue, int alpha )
{
	_drawColor[0] = red;
	_drawColor[1] = green;
	_drawColor[2] = blue;
	_drawColor[3] = alpha;
}

void EngineSurface::drawSetTextColor( int red, int green, int blue, int alpha )
{
	if ( _drawTextColor[0] == red && _drawTextColor[1] == green && _drawTextColor[2] == blue
		&& _drawTextColor[3] == alpha )
		return;

	drawFlushText();
	_drawTextColor[0] = red;
	_drawTextColor[1] = green;
	_drawTextColor[2] = blue;
	_drawTextColor[3] = alpha;
}

void EngineSurface::drawSetTextPos( int x, int y )
{
	_drawTextPos[0] = x;
	_drawTextPos[1] = y;
}

void EngineSurface::drawGetTextPos( int& x, int& y )
{
	x = _drawTextPos[0];
	y = _drawTextPos[1];
}

EngineSurfaceTextures::EngineSurfaceTextures()
{
	InitTextures();
}

EngineSurfaceTextures::~EngineSurfaceTextures()
{
	FreeTextures();
}

void EngineSurfaceTextures::InitTextures()
{
	memset( hash, 0, sizeof( hash ) );
}

void EngineSurfaceTextures::FreeTextures()
{
	for ( int i = 0; i < TEXTURE_HASH_SIZE; i++ )
	{
		vgui_texture_t* texture = hash[i];
		while ( texture )
		{
			vgui_texture_t* next = texture->next;
            delete[] texture->prgba;
			delete texture;
			texture = next;
		}
		hash[i] = NULL;
	}
}

int EngineSurfaceTextures::GetHashVal( int id )
{
	return static_cast<unsigned int>(id) % TEXTURE_HASH_SIZE;
}

namespace {
constexpr std::size_t TextureChannels = 4;

void NormalizeTransparentTexels(unsigned char* pixels, std::size_t bytes) {
#if INTPTR_MAX > INT32_MAX
    constexpr std::size_t AlphaChannel = 3;
    for (std::size_t offset = 0; offset < bytes; offset += TextureChannels) {
        // The retained SW text rasterizer uses a nonzero packed RGBA mask.
        // Native FreeType glyphs can contain white RGB with zero coverage.
        // Normalize only fully transparent storage; keep coverage and all
        // visible RGB bytes intact for the retained image/additive equations.
        if (pixels[offset + AlphaChannel] == 0) {
            std::memset(pixels + offset, 0, TextureChannels);
        }
    }
#else
    (void)pixels;
    (void)bytes;
#endif
}


bool TextureBytes(int width, int height, std::size_t& bytes) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    // The retained rasterizer computes byte offsets in signed int arithmetic.
    constexpr std::size_t MaximumTextureBytes = (std::numeric_limits<int>::max)();
    if (static_cast<std::size_t>(width) > MaximumTextureBytes / TextureChannels) {
        return false;
    }
    const std::size_t row = static_cast<std::size_t>(width) * TextureChannels;
    if (row > MaximumTextureBytes / static_cast<std::size_t>(height)) {
        return false;
    }
    bytes = row * static_cast<std::size_t>(height);
    return true;
}
}

void EngineSurfaceTextures::Insert(int id, int wide, int tall, const char* pixels) {
    std::size_t bytes = 0;
    if (id <= 0 || !pixels || !TextureBytes(wide, tall, bytes)) {
        return;
    }
    std::unique_ptr<unsigned char[]> storage(new unsigned char[bytes]);
    std::memcpy(storage.get(), pixels, bytes);
    NormalizeTransparentTexels(storage.get(), bytes);
    std::unique_ptr<vgui_texture_t> texture(new vgui_texture_t{});
    texture->id = id;
    texture->prgba = storage.release();
    texture->wide = wide;
    texture->tall = tall;
    Remove(id);
    const int bucket = GetHashVal(id);
    texture->next = hash[bucket];
    hash[bucket] = texture.release();
}

void EngineSurfaceTextures::Remove( int id )
{
	vgui_texture_t** link = &hash[GetHashVal( id )];
	while ( *link )
	{
		vgui_texture_t* texture = *link;
		if ( texture->id == id )
		{
			*link = texture->next;
            delete[] texture->prgba;
			delete texture;
			return;
		}
		link = &texture->next;
	}
}

vgui_texture_t* EngineSurfaceTextures::GetTexture( int id )
{
	vgui_texture_t* texture = hash[GetHashVal( id )];
	while ( texture && texture->id != id )
		texture = texture->next;
	return texture;
}

void EngineSurface::createRenderPlat()
{
	_renderPlat = new EngineSurfaceRenderPlat;
	memset( _renderPlat, 0, sizeof( *_renderPlat ) );
	_renderPlat->textures = new EngineSurfaceTextures;
}

void EngineSurface::deleteRenderPlat() {
    if (_renderPlat) {
        delete _renderPlat->textures;
        delete _renderPlat;
        _renderPlat = nullptr;
    }
}

void EngineSurface::resetViewPort()
{
	_renderPlat->left = 0;
	_renderPlat->top = 0;
	_renderPlat->right = 0;
	_renderPlat->bottom = 0;
}

void EngineSurface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	if ( _drawColor[3] == 0 )
		Draw_FillRGB( g_TranslateX + _renderPlat->left + x0, g_TranslateY + _renderPlat->top + y0,
			x1 - x0, y1 - y0, _drawColor[0], _drawColor[1], _drawColor[2] );
	else if ( _drawColor[3] != 255 )
		Draw_FillRGBABlend( g_TranslateX + _renderPlat->left + x0, g_TranslateY + _renderPlat->top + y0,
			x1 - x0, y1 - y0, _drawColor[0], _drawColor[1], _drawColor[2], 255 - _drawColor[3] );
}

void EngineSurface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y0, g_TranslateX + x1, g_TranslateY + y0 + 1 );
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y1 - 1, g_TranslateX + x1, g_TranslateY + y1 );
	drawFilledRect( g_TranslateX + x0, g_TranslateY + y0 + 1, g_TranslateX + x0 + 1, g_TranslateY + y1 - 1 );
	drawFilledRect( g_TranslateX + x1 - 1, g_TranslateY + y0 + 1, g_TranslateX + x1, g_TranslateY + y1 - 1 );
}

void EngineSurface::drawLine( int x0, int y0, int x1, int y1 )
{
	Draw_LineRGB( x0 + _renderPlat->left + g_TranslateX, y0 + _renderPlat->top + g_TranslateY,
		g_TranslateX + _renderPlat->left + x1, g_TranslateY + _renderPlat->top + y1,
		_drawTextColor[0], _drawTextColor[1], _drawTextColor[2] );
}

void EngineSurface::drawPolyLine( int* x, int* y, int count )
{
	for ( int i = 0; i < count - 1; i++ )
	{
		Draw_LineRGB( g_TranslateX + x[i] + _renderPlat->left, g_TranslateY + y[i] + _renderPlat->top,
			g_TranslateX + _renderPlat->left + x[i + 1], g_TranslateY + _renderPlat->top + y[i + 1],
			_drawTextColor[0], _drawTextColor[1], _drawTextColor[2] );
	}
}

void EngineSurface::drawTexturedPolygon( vgui2::VGuiVertex*, int )
{
}

void EngineSurface::drawSetTextureRGBA(int id, const char* pixels, int wide, int tall, int, int) {
    if (_renderPlat->textures) {
        const bool bound = _renderPlat->current && _renderPlat->current->id == id;
        _renderPlat->textures->Insert(id, wide, tall, pixels);
        if (bound) {
            _renderPlat->current = _renderPlat->textures->GetTexture(id);
        }
    }
}

void EngineSurface::drawSetTextureBGRA(int id, const char* pixels, int wide, int tall, int filter, int reload) {
#if INTPTR_MAX > INT32_MAX
    std::size_t bytes = 0;
    if (id <= 0 || !pixels || !TextureBytes(wide, tall, bytes)) {
        return;
    }
    std::unique_ptr<unsigned char[]> rgba(new unsigned char[bytes]);
    for (std::size_t offset = 0; offset < bytes; offset += TextureChannels) {
        rgba[offset] = static_cast<unsigned char>(pixels[offset + 2]);
        rgba[offset + 1] = static_cast<unsigned char>(pixels[offset + 1]);
        rgba[offset + 2] = static_cast<unsigned char>(pixels[offset]);
        rgba[offset + 3] = static_cast<unsigned char>(pixels[offset + 3]);
    }
    drawSetTextureRGBA(id, reinterpret_cast<const char*>(rgba.get()), wide, tall, filter, reload);
#else
    // Win32 retains its original byte interpretation, but shares replacement
    // ownership repair with RGBA so the currently bound pointer cannot dangle.
    drawSetTextureRGBA(id, pixels, wide, tall, filter, reload);
#endif
}

void EngineSurface::drawSetTexture( int id )
{
	_renderPlat->current = _renderPlat->textures ? _renderPlat->textures->GetTexture( id ) : NULL;
}

void EngineSurface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	int x = g_TranslateX + x0 + _renderPlat->left;
	int y = g_TranslateY + y0 + _renderPlat->top;
	int wide = x1 - x0;
	int tall = y1 - y0;
	if ( wide == texture->wide && tall == texture->tall )
		Draw_RGBAImage( x, y, 0, 0, wide, tall, wide, tall, texture->prgba, 0,
			(byte)_drawColor[0], (byte)_drawColor[1], (byte)_drawColor[2], (byte)~_drawColor[3] );
	else
		Draw_ScaledRGBAImage( x, y, wide, tall, 0, 0, texture->wide, texture->tall,
			texture->wide, texture->tall, texture->prgba );
}

void EngineSurface::drawTexturedRectAdd( int x0, int y0, int x1, int y1 )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	int x = g_TranslateX + x0 + _renderPlat->left;
	int y = g_TranslateY + y0 + _renderPlat->top;
	int wide = x1 - x0;
	int tall = y1 - y0;
	if ( wide == texture->wide && tall == texture->tall )
		Draw_RGBAImageAdditive( x, y, 0, 0, wide, tall, wide, tall, texture->prgba, 0,
			(byte)_drawColor[0], (byte)_drawColor[1], (byte)_drawColor[2], (byte)~_drawColor[3] );
	else
		Draw_ScaledRGBAImage( x, y, wide, tall, 0, 0, texture->wide, texture->tall,
			texture->wide, texture->tall, texture->prgba );
}

void EngineSurface::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float, float )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	float srcY = (float)texture->tall * t0;
	float srcX = (float)texture->wide * s0;
	Draw_RGBAImage( g_TranslateX + _renderPlat->left + x, g_TranslateY + _renderPlat->top + y,
		(int)srcX, (int)srcY, wide, tall, texture->wide, texture->tall, texture->prgba, 1,
		(byte)_drawTextColor[0], (byte)_drawTextColor[1], (byte)_drawTextColor[2], 255 );
}

void EngineSurface::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float, float )
{
	vgui_texture_t* texture = _renderPlat->current;
	if ( !texture )
		return;

	float srcY = (float)texture->tall * t0;
	float srcX = (float)texture->wide * s0;
	Draw_RGBAImageAdditive( g_TranslateX + _renderPlat->left + x, g_TranslateY + _renderPlat->top + y,
		(int)srcX, (int)srcY, wide, tall, texture->wide, texture->tall, texture->prgba, 1,
		(byte)_drawTextColor[0], (byte)_drawTextColor[1], (byte)_drawTextColor[2], (byte)~_drawTextColor[3] );
}

void EngineSurface::drawFlushText()
{
}

namespace {
bool TextureRegion(const vgui_texture_t* texture, int x, int y, int width, int height,
    const unsigned char* pixels) {
    return texture && pixels && x >= 0 && y >= 0 && width > 0 && height > 0
        && x <= texture->wide && y <= texture->tall
        && width <= texture->wide - x && height <= texture->tall - y;
}
}

void EngineSurface::drawSetSubTextureRGBA(int id, int x, int y, const unsigned char* pixels, int wide, int tall) {
    vgui_texture_t* texture = _renderPlat->textures->GetTexture(id);
    if (!TextureRegion(texture, x, y, wide, tall, pixels)) {
        return;
    }
    const std::size_t rowBytes = TextureChannels * static_cast<std::size_t>(wide);
    for (int row = 0; row < tall; ++row) {
        const std::size_t offset = (static_cast<std::size_t>(y + row) * texture->wide + x) * TextureChannels;
        std::memcpy(texture->prgba + offset, pixels, rowBytes);
        NormalizeTransparentTexels(texture->prgba + offset, rowBytes);
        pixels += rowBytes;
    }
    _renderPlat->current = texture;
}

void EngineSurface::drawUpdateRegionTextureBGRA(int id, int x, int y, const unsigned char* pixels,
    int wide, int tall) {
#if INTPTR_MAX > INT32_MAX
    vgui_texture_t* texture = _renderPlat->textures->GetTexture(id);
    if (!TextureRegion(texture, x, y, wide, tall, pixels)) {
        return;
    }
    for (int row = 0; row < tall; ++row) {
        const std::size_t offset = (static_cast<std::size_t>(y + row) * texture->wide + x) * TextureChannels;
        unsigned char* target = texture->prgba + offset;
        // HTML passes its complete frame, not a packed dirty rectangle.
        const unsigned char* source = pixels + offset;
        for (int column = 0; column < wide; ++column) {
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            target[3] = source[3];
            target += TextureChannels;
            source += TextureChannels;
        }
        NormalizeTransparentTexels(texture->prgba + offset, static_cast<std::size_t>(wide) * TextureChannels);
    }
    _renderPlat->current = texture;
#else
    (void)id;
    (void)x;
    (void)y;
    (void)pixels;
    (void)wide;
    (void)tall;
#endif
}

void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )
{
	vgui_texture_t* texture = _renderPlat->textures->GetTexture( id );
	if ( texture && !forceReload )
	{
		_renderPlat->current = texture;
		return;
	}

	byte* pixels = (byte*)malloc( 1024 * 1024 );
	int wide;
	int tall;
#ifdef _WIN32
	char name[MAX_PATH];
#else
	char name[4096];
#endif
	_snprintf( name, sizeof( name ), "%s.tga", filename );
	if ( LoadTGA( name, pixels, 1024 * 1024, &wide, &tall ) )
	{
		drawSetTextureRGBA( id, (const char*)pixels, wide, tall, hardwareFilter, 0 );
	}
	else
	{
		_snprintf( name, sizeof( name ), "%s.bmp", filename );
		FileHandle_t file = FS_Open( name, "rb" );
		if ( file )
		{
			VGui_LoadBMP( file, pixels, 1024 * 1024, &wide, &tall );
			drawSetTextureRGBA( id, (const char*)pixels, wide, tall, hardwareFilter, 0 );
		}
	}
	texture = _renderPlat->textures->GetTexture( id );
	free( pixels );
	if ( texture )
		_renderPlat->current = texture;
}

void EngineSurface::drawGetTextureSize( int id, int& wide, int& tall )
{
	vgui_texture_t* texture = _renderPlat->textures->GetTexture( id );
	if ( texture )
	{
		wide = texture->wide;
		tall = texture->tall;
	}
	else
	{
		wide = 0;
		tall = 0;
	}
}

int EngineSurface::isTextureIdValid( int id )
{
	return _renderPlat->textures->GetTexture( id ) != NULL;
}

void EngineSurface::pushMakeCurrent(int *insets, int *absoluteExtents, int *clipRect, bool translateToScreenSpace) {
    int windowX = 0;
    int windowY = 0;
    if (translateToScreenSpace && VideoMode_IsWindowed()) {
        SDL_GetWindowPosition(*pmainwindow, &windowX, &windowY);
    }

    int wide;
    int tall;
    if (VideoMode_IsWindowed()) {
        SDL_GetWindowSizeInPixels(*pmainwindow, &wide, &tall);
    } else {
        VideoMode_GetCurrentVideoMode(&wide, &tall, NULL);
    }

    _renderPlat->left = absoluteExtents[0] - windowX;
    _renderPlat->top = absoluteExtents[1] - windowY;
    _renderPlat->right = absoluteExtents[2] - windowX;
    _renderPlat->bottom = absoluteExtents[3] - windowY;
    int left = clipRect[0] - windowX;
    int top = clipRect[1] - windowY;
    int right = clipRect[2] - windowX;
    int bottom = clipRect[3] - windowY;
    g_TranslateX = insets[0];
    g_TranslateY = insets[1];
    if (left != 0 || top != 0 || right != wide || bottom != tall) {
        EnableScissorTest(left - insets[0], top - insets[1], insets[0] + right - left, insets[1] + bottom - top);
    }
}

void EngineSurface::popMakeCurrent()
{
	DisableScissorTest();
	resetViewPort();
}

void VGui_ViewportPaintBackground( int* extents )
{
	g_engdstAddrs.VGui_ViewportPaintBackground( &extents );
	V_RenderView();
	FillBackGround();
	if ( !scr_drawloading && ( cl.intermission != 1 || key_dest != key_game ) )
		SCR_DrawConsole();
	if ( cls.state == ca_active )
		ClientDLL_HudRedraw( cl.intermission == 1 );
}

#if defined(__APPLE__)
bool EngineSurfaceTextures::RetireAtlasPage(int id) {
    if (id <= 0) {
        return false;
    }
    vgui_texture_t** link = &hash[GetHashVal(id)];
    while (*link) {
        vgui_texture_t* texture = *link;
        if (texture->id == id) {
            *link = texture->next;
            delete[] texture->prgba;
            delete texture;
            return true;
        }
        link = &texture->next;
    }
    return true;
}
namespace cse_vgui1_native {
bool UploadEngineFontPage(IEngineSurface* surface, int id, const unsigned char* pixels, std::size_t bytes) noexcept {
    constexpr int Side = 512;
    constexpr std::size_t PageBytes = static_cast<std::size_t>(Side) * Side * 4;
    if (!FindOwnedEngineSurface(surface) || id <= 0 || !pixels || bytes != PageBytes) {
        return false;
    }
    return UploadOwnedInterface(surface, id, pixels, Side);
}
// Kept as an EngineSurface member only for private render-plat access; this adds
// no public virtual slot or storage. The free bridge below uses exact instances.
}
#endif

#if defined(__APPLE__)
bool EngineSurface::RetireNativeFontPage(int id) noexcept {
    if (!_renderPlat || !_renderPlat->textures || id <= 0) {
        return false;
    }
    if (_renderPlat->current && _renderPlat->current->id == id) {
        _renderPlat->current = nullptr;
    }
    return _renderPlat->textures->RetireAtlasPage(id);
}
namespace cse_vgui1_native {
bool RetireEngineFontPage(IEngineSurface* surface, int id, bool) noexcept {
    auto* owned = FindOwnedEngineSurface(surface);
    return owned && owned->RetireNativeFontPage(id);
}
}
#endif
