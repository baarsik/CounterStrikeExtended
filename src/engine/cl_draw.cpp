#if defined( SW_RENDERER )
#include "r_local.h"
#include "draw.h"
#else
#include "glquake.h"
#endif
#include "../common/image_codec.h"
#include "cl_draw.h"
#include "vid.h"
#include "engine_apiproxy.h"
#include "gl_model.h"
#include "common.h"
#include "mem.h"
#include "sys.h"
#include "conprint.h"

#define SPR_MAX_SPRITES 256

#if !defined(SW_RENDERER)
struct WebPHudSprite {
    model_t model;
    msprite_t sprite;
    mspriteframe_t frame;
};
#endif

typedef struct spritelist_s
{
	model_t* pSprite;
	char* pName;
	int frameCount;
#if !defined(SW_RENDERER)
    WebPHudSprite* webp;
#endif
} SPRITELIST;

SPRITELIST* gSpriteList;
int gSpriteCount;

msprite_t* gpSprite;

#if !defined(SW_RENDERER)
static bool gSpriteHasAlpha;
#endif

qboolean gSpriteMipMap = TRUE;
unsigned short gSpritePalette[256];

HSPRITE ghCrosshair;
wrect_t gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;

extern int filterMode;
extern float filterColorRed;
extern float filterColorGreen;
extern float filterColorBlue;
extern float filterBrightness;

void EnableScissorTest( int x, int y, int width, int height );
void DisableScissorTest();

void Draw_SpriteFrame( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameHoles( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameAdditive( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect );
void Draw_SpriteFrameGeneric( mspriteframe_t* frame, unsigned short* palette, int x, int y, const wrect_t* rect,
	int src, int dest, int width, int height );

#if !defined(SW_RENDERER)
static WebPHudSprite* LoadWebPHudSprite(const char* name) {
    constexpr int maximumSide = 1024;
    constexpr auto maximumPixelBytes = maximumSide * maximumSide * cse::image::RgbaChannels;
    constexpr unsigned int maximumEncodedBytes = 8 * 1024 * 1024;
    if (Q_strlen(name) >= sizeof(model_t::name)) {
        return nullptr;
    }
    FileHandle_t file = FS_Open(name, "rb");
    if (!file) {
        return nullptr;
    }
    const auto length = FS_Size(file);
    if (!length || length > maximumEncodedBytes) {
        FS_Close(file);
        return nullptr;
    }
    std::vector<byte> encoded(length);
    const int bytesRead = FS_Read(encoded.data(), static_cast<int>(length), 1, file);
    FS_Close(file);

    cse::image::Image image;
    if (bytesRead != static_cast<int>(length) ||
        !cse::image::DecodeWebP(encoded.data(), encoded.size(), image, maximumPixelBytes, maximumSide)) {
        return nullptr;
    }
    char textureName[sizeof(model_t::name)];
    Q_strcpy(textureName, name);
    const int texture = GL_LoadTexture(textureName, GLT_HUDSPRITE, image.width, image.height,
        image.rgba.data(), FALSE, TEX_TYPE_RGBA, nullptr);
    if (!texture) {
        return nullptr;
    }

    // Own the single HUD frame independently of the map's model cache.
    auto* hud = static_cast<WebPHudSprite*>(Mem_ZeroMalloc(sizeof(WebPHudSprite)));
    Q_strcpy(hud->model.name, name);
    hud->model.type = mod_sprite;
    hud->model.numframes = 1;
    hud->model.cache.data = &hud->sprite;
    hud->sprite.type = SPR_VP_PARALLEL;
    hud->sprite.texFormat = SPR_ALPHTEST;
    hud->sprite.maxwidth = image.width;
    hud->sprite.maxheight = image.height;
    hud->sprite.numframes = 1;
    hud->sprite.frames[0] = {SPR_SINGLE, &hud->frame};
    hud->frame.width = image.width;
    hud->frame.height = image.height;
    hud->frame.right = static_cast<float>(image.width);
    hud->frame.down = -static_cast<float>(image.height);
    hud->frame.gl_texturenum = texture;
    return hud;
}
#endif

void SPR_Init()
{
	if ( gSpriteList )
		return;

	ghCrosshair = 0;
	gSpriteCount = SPR_MAX_SPRITES;
	gSpriteList = (SPRITELIST*)Mem_ZeroMalloc( sizeof( SPRITELIST ) * SPR_MAX_SPRITES );
	gpSprite = NULL;
}

HSPRITE SPR_Load( const char* name )
{
    g_engdstAddrs.pfnSPR_Load(&name);
    if (!name || !*name || !gSpriteList || gSpriteCount <= 0) {
        return 0;
    }

    int index = 0;
    for (; index < gSpriteCount; ++index) {
        const auto* loadedName = gSpriteList[index].pName;
        if (!loadedName || !Q_strcasecmp(name, loadedName)) {
            break;
        }
    }
    if (index == gSpriteCount) {
        Con_DPrintf("Cannot allocate more than %d HUD sprites\n", SPR_MAX_SPRITES);
        return 0;
    }

    auto& slot = gSpriteList[index];
    if (cse::image::HasWebPExtension(name)) {
#if defined(SW_RENDERER)
        return 0;
#else
        if (!slot.webp) {
            slot.webp = LoadWebPHudSprite(name);
        }
        slot.pSprite = slot.webp ? &slot.webp->model : nullptr;
#endif
    } else {
        gSpriteMipMap = FALSE;
        slot.pSprite = Mod_ForName(name, FALSE, TRUE);
        gSpriteMipMap = TRUE;
    }
    if (!slot.pSprite) {
        return 0;
    }
    if (!slot.pName) {
        slot.pName = static_cast<char*>(Mem_Malloc(Q_strlen(name) + 1));
        Q_strcpy(slot.pName, name);
    }
    slot.frameCount = ModelFrameCount(slot.pSprite);
    return index + 1;
}

static void SPR_FreeList( bool unloadTextures )
{
	int i;

	if ( !host_initialized )
		return;

	if ( gSpriteList )
	{
		for ( i = 0; i < gSpriteCount; i++ )
		{
#if !defined(SW_RENDERER)
            if (gSpriteList[i].webp) {
                GL_UnloadTexture(gSpriteList[i].pName);
                Mem_Free(gSpriteList[i].webp);
            } else
#endif
			if ( unloadTextures && gSpriteList[i].pSprite )
				Mod_UnloadSpriteTextures( gSpriteList[i].pSprite );

			if ( gSpriteList[i].pName )
				Mem_Free( gSpriteList[i].pName );
		}

		Mem_Free( gSpriteList );
	}

	gSpriteList = NULL;
	gSpriteCount = 0;
	gpSprite = NULL;
	ghCrosshair = 0;
}

void SPR_Shutdown()
{
	SPR_FreeList( true );
}

void SPR_Shutdown_NoModelFree()
{
	SPR_FreeList( false );
}

SPRITELIST* SPR_Get( HSPRITE sprite )
{
	int index;

	index = sprite - 1;
	if ( index < 0 || index >= gSpriteCount )
		return NULL;

	return &gSpriteList[index];
}

msprite_t* SPR_Pointer( SPRITELIST* list )
{
	return list->pSprite ? static_cast<msprite_t*>(list->pSprite->cache.data) : nullptr;
}

void SPR_Set( HSPRITE sprite, int r, int g, int b )
{
	SPRITELIST* list;

	g_engdstAddrs.pfnSPR_Set( &sprite, &r, &g, &b );

	list = SPR_Get( sprite );
	gpSprite = list ? SPR_Pointer(list) : nullptr;
#if !defined(SW_RENDERER)
    gSpriteHasAlpha = list && list->webp;
#endif
	if ( !list )
		return;

	if ( gpSprite )
#if defined( SW_RENDERER )
		UnpackPalette( gSpritePalette, (word*)( (byte*)gpSprite + gpSprite->paloffset ), r, g, b );
#else
		qglColor4f( (float)( r / 255.0 ), (float)( g / 255.0 ), (float)( b / 255.0 ), 1.0f );
#endif
}

int SPR_Width( HSPRITE sprite, int frame )
{
	SPRITELIST* list;
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Width( &sprite, &frame );

	list = SPR_Get( sprite );
	if ( list )
	{
		pFrame = R_GetSpriteFrame( SPR_Pointer( list ), frame );
		if ( pFrame )
			return pFrame->width;
	}

	return 0;
}

int SPR_Height( HSPRITE sprite, int frame )
{
	SPRITELIST* list;
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Height( &sprite, &frame );

	list = SPR_Get( sprite );
	if ( list )
	{
		pFrame = R_GetSpriteFrame( SPR_Pointer( list ), frame );
		if ( pFrame )
			return pFrame->height;
	}

	return 0;
}

int SPR_Frames( HSPRITE sprite )
{
	SPRITELIST* list;

	g_engdstAddrs.pfnSPR_Frames( &sprite );

	list = SPR_Get( sprite );
	if ( list )
		return list->frameCount;

	return 0;
}

model_t* SPR_GetModelPointer( HSPRITE sprite )
{
	SPRITELIST* list;

	list = SPR_Get( sprite );
	if ( list )
		return list->pSprite;

	return NULL;
}

void SPR_Draw( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_Draw( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
#if !defined(SW_RENDERER)
    if (pFrame && gSpriteHasAlpha) {
        Draw_SpriteFrameGeneric(pFrame, gSpritePalette, x, y, rect, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
            pFrame->width, pFrame->height);
        return;
    }
#endif
	if ( pFrame )
		Draw_SpriteFrame( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_Draw error:  invalid frame\n" );
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawHoles( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
#if !defined(SW_RENDERER)
    if (pFrame && gSpriteHasAlpha) {
        Draw_SpriteFrameGeneric(pFrame, gSpritePalette, x, y, rect, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
            pFrame->width, pFrame->height);
        return;
    }
#endif
	if ( pFrame )
		Draw_SpriteFrameHoles( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_DrawHoles error:  invalid frame\n" );
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t* rect )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawAdditive( &frame, &x, &y, &rect );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
#if !defined(SW_RENDERER)
    if (pFrame && gSpriteHasAlpha) {
        Draw_SpriteFrameGeneric(pFrame, gSpritePalette, x, y, rect, GL_SRC_ALPHA, GL_ONE,
            pFrame->width, pFrame->height);
        return;
    }
#endif
	if ( pFrame )
		Draw_SpriteFrameAdditive( pFrame, gSpritePalette, x, y, rect );
	else
		Con_DPrintf( "Client.dll SPR_DrawAdditive error:  invalid frame\n" );
}

void SPR_DrawGeneric( int frame, int x, int y, const wrect_t* rect, int src, int dest, int width, int height )
{
	mspriteframe_t* pFrame;

	g_engdstAddrs.pfnSPR_DrawGeneric( &frame, &x, &y, &rect, &src, &dest, &width, &height );

	if ( !gpSprite || x >= (int)vid.width || y >= (int)vid.height )
		return;

	pFrame = R_GetSpriteFrame( gpSprite, frame );
	if ( pFrame )
		Draw_SpriteFrameGeneric( pFrame, gSpritePalette, x, y, rect, src, dest, width, height );
	else
		Con_DPrintf( "Client.dll SPR_DrawGeneric error: invalid frame\n" );
}

void SPR_EnableScissor( int x, int y, int width, int height )
{
	g_engdstAddrs.pfnSPR_EnableScissor( &x, &y, &width, &height );
	EnableScissorTest( x, y, width, height );
}

void SPR_DisableScissor()
{
	g_engdstAddrs.pfnSPR_DisableScissor();
	DisableScissorTest();
}

void SetCrosshair( HSPRITE sprite, wrect_t rect, int r, int g, int b )
{
	g_engdstAddrs.pfnSetCrosshair( &sprite, &rect, &r, &g, &b );

	ghCrosshair = sprite;
	gCrosshairRc = rect;
	gCrosshairR = r;
	gCrosshairG = g;
	gCrosshairB = b;
}

void DrawCrosshair( int x, int y )
{
	if ( !ghCrosshair )
		return;

	SPR_Set( ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB );

	x -= ( gCrosshairRc.right - gCrosshairRc.left ) / 2;
	y -= ( gCrosshairRc.bottom - gCrosshairRc.top ) / 2;
	SPR_DrawHoles( 0, x, y, &gCrosshairRc );
}

void SetFilterMode( int mode )
{
	g_engdstAddrs.pfnSetFilterMode( &mode );
	filterMode = mode;
}

void SetFilterColor( float red, float green, float blue )
{
	g_engdstAddrs.pfnSetFilterColor( &red, &green, &blue );
	filterColorRed = red;
	filterColorGreen = green;
	filterColorBlue = blue;
}

void SetFilterBrightness( float brightness )
{
	g_engdstAddrs.pfnSetFilterBrightness( &brightness );
	filterBrightness = brightness;
}

#if defined( SW_RENDERER )
void UnpackPalette( unsigned short* dest, unsigned short* source, int r, int g, int b )
{
	int red = ( ( (unsigned int)r * 384 ) & 0x1FE00 ) >> 1;
	int green = ( ( (unsigned int)g * 384 ) & 0x1FE00 ) >> 1;
	int blue = ( ( (unsigned int)b * 384 ) & 0x1FE00 ) >> 1;
	for ( int i = 0; i < 256; i++ )
	{
		dest[i] = red_64klut[red + source[4 * i]] | green_64klut[green + source[4 * i + 1]]
			| blue_64klut[blue + source[4 * i + 2]];
	}
}
#else
void UnpackPalette( unsigned short*, unsigned short*, int, int, int )
{
}
#endif
