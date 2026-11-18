#include "r_local.h"
#include "d_local.h"
#include "engine_color.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "pm_defs.h"
#include "sys.h"
#include "zone.h"
#include "engine_image.h"
#include "../pm_shared/pm_movevars.h"

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

#define MAX_CLIP_VERTS	64

static const char* suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
static pixel_t* gSkies[6];
static pixel_t* gFilteredSkies[6];
static int skytexorder[6] = { 0, 2, 1, 3, 4, 5 };
static int gFakePlaneType[6] = { 1, -1, -2, 2, 3, -3 };
static msurface_t gFakeSurface[6];
static texture_t gFakeTex;
static mtexinfo_t gFakeTexInfo[6];
static mplane_t gFakePlanes[6];
static bedge_t gFakeEdges[4];
static mvertex_t gFakeVerts[4];
static int gLoadSky, gDrawSky;
static cl_entity_t gFakeEntity;
static float skymins[2][6], skymaxs[2][6];

vec3_t skyclip[6] =
{
	{ 1, 1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0, 1, 1 },
	{ 1, 0, 1 },
	{ -1, 0, 1 }
};

int st_to_vec[6][3] =
{
	{ 3, -1, 2 },
	{ -3, 1, 2 },
	{ 1, 3, 2 },
	{ -1, -3, 2 },
	{ -2, -1, 3 },
	{ 2, -1, -3 }
};

int vec_to_st[6][3] =
{
	{ -2, 3, 1 },
	{ 2, 3, -1 },
	{ 1, 3, 2 },
	{ -1, 3, -2 },
	{ -2, -1, 3 },
	{ -2, 1, -3 }
};

int fgetLittleShort( byte** p )
{
	byte* data = *p;
	*p += 2;
	return (short)( data[0] + ( data[1] << 8 ) );
}

int fgetLittleLong( byte** p )
{
	byte* data = *p;
	*p += 4;
	return data[0] + ( data[1] << 8 ) + ( data[2] << 16 ) + ( data[3] << 24 );
}

short PutDitheredRGB( colorVec* color )
{
	color->r |= ( color->r & 0xC0 ) >> 6;
	color->g |= ( color->g & 0xC0 ) >> 6;
	color->b |= ( color->b & 0xC0 ) >> 6;
	if ( is15bit )
		return (short)( ( (short)color->b >> 3 ) | ( ( color->g << 2 ) & COLOR_GREENMASK_555 )
			| ( ( color->r << 7 ) & COLOR_REDMASK_555 ) );
	return (short)( ( (short)color->b >> 3 ) | ( ( color->g << 3 ) & COLOR_GREENMASK_565 )
		| ( ( color->r << 8 ) & COLOR_REDMASK_565 ) );
}

static byte* R_CopyWebPImage(const cse::image::Image& decoded, byte* buffer, int* width, int* height,
                            qboolean output32bit) {
    constexpr std::size_t SoftwarePixelBytes = sizeof(unsigned short);
    const std::size_t capacityScale = output32bit ? 1 : cse::image::RgbaChannels / SoftwarePixelBytes;
    const std::size_t outputBytes = decoded.rgba.size() / capacityScale;
    if (!buffer) {
        buffer = static_cast<byte*>(Hunk_AllocName(static_cast<int>(outputBytes), "SKYBOX"));
    }
    if (output32bit) {
        std::memcpy(buffer, decoded.rgba.data(), outputBytes);
    } else {
        for (std::size_t offset = 0; offset < decoded.rgba.size(); offset += cse::image::RgbaChannels) {
            colorVec color;
            color.r = decoded.rgba[offset];
            color.g = decoded.rgba[offset + 1];
            color.b = decoded.rgba[offset + 2];
            color.a = decoded.rgba[offset + 3];
            const short pixel = PutDitheredRGB(&color);
            std::memcpy(buffer + offset / capacityScale, &pixel, sizeof(pixel));
        }
    }
    if (width) {
        *width = decoded.width;
    }
    if (height) {
        *height = decoded.height;
    }
    return buffer;
}

static byte* R_LoadTGAImage(char* filename, byte* buffer, int bufferSize, int* width, int* height,
                            qboolean output32bit, int requiredDimension, bool preservePackedRle = false) {
    if (width) {
        *width = 0;
    }
    if (height) {
        *height = 0;
    }
    if (buffer && bufferSize <= 0) {
        return NULL;
    }
    constexpr std::size_t SoftwarePixelBytes = sizeof(unsigned short);
    const std::size_t capacityScale = output32bit ? 1 : cse::image::RgbaChannels / SoftwarePixelBytes;
    const std::size_t dimensionCapacity = requiredDimension > 0
        ? static_cast<std::size_t>(requiredDimension) * requiredDimension * cse::image::RgbaChannels
        : cse::image::MaximumDecodedBytes;
    const std::size_t bufferCapacity = buffer ? static_cast<std::size_t>(bufferSize) * capacityScale : dimensionCapacity;
    const std::size_t capacity = bufferCapacity < dimensionCapacity ? bufferCapacity : dimensionCapacity;
    cse::image::Image decoded;
    if (cse::engine_image::LoadWebP(filename, decoded, capacity)
        && (!requiredDimension || (decoded.width == requiredDimension && decoded.height == requiredDimension))) {
        return R_CopyWebPImage(decoded, buffer, width, height, output32bit);
    }
    if (cse::image::HasWebPExtension(filename)) {
        return NULL;
    }
    int fileLength = 0;
    byte* file = cse::engine_image::LoadLegacyTarga(filename, &fileLength);
    if (!file) {
        return NULL;
    }
    if (fileLength > 0 && cse::image::IsWebP(file, static_cast<std::size_t>(fileLength))) {
        const bool loaded = cse::image::DecodeWebP(file, static_cast<std::size_t>(fileLength), decoded, capacity)
            && (!requiredDimension || (decoded.width == requiredDimension && decoded.height == requiredDimension));
        COM_FreeFile(file);
        return loaded ? R_CopyWebPImage(decoded, buffer, width, height, output32bit) : NULL;
    }
    const auto fail = [&](const char* message) -> byte* {
        COM_FreeFile(file);
        Con_DPrintf("%s", message);
        return NULL;
    };
    constexpr std::size_t HeaderBytes = 18;
    constexpr int RawImageType = 2;
    constexpr int RleImageType = 10;
    constexpr int RgbBits = 24;
    constexpr int RgbaBits = 32;
    constexpr int BitsPerByte = 8;
    constexpr byte OpaqueAlpha = 255;
    constexpr byte RleRunFlag = 0x80;
    constexpr byte RleCountMask = 0x7f;
    if (fileLength < 0 || static_cast<std::size_t>(fileLength) < HeaderBytes) {
        return fail("R_LoadTGA: file shorter than TGA file header length\n");
    }
    const int imageType = file[2];
    const std::size_t columns = file[12] + (file[13] << BitsPerByte);
    const std::size_t rows = file[14] + (file[15] << BitsPerByte);
    const int pixelSize = file[16];
    if (requiredDimension > 0 && (columns != static_cast<std::size_t>(requiredDimension)
                                  || rows != static_cast<std::size_t>(requiredDimension))) {
        return fail("R_LoadTGA: sky face has invalid dimensions\n");
    }
    if (imageType != RawImageType && imageType != RleImageType) {
        return fail("R_LoadTGA: Only type 2 and 10 targa RGB images supported\n");
    }
    if (file[1] || (pixelSize != RgbBits && pixelSize != RgbaBits)) {
        return fail("R_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
    }
    // The low-level R_LoadTGA entry historically returns packed pixels for RLE images.
    const bool rgbaOutput = output32bit && (!preservePackedRle || imageType == RawImageType);
    const std::size_t outputPixelBytes = rgbaOutput ? cse::image::RgbaChannels : SoftwarePixelBytes;
    if (!columns || !rows || columns > static_cast<std::size_t>(INT_MAX) / outputPixelBytes / rows) {
        return fail("R_LoadTGA: invalid or oversized texture dimensions\n");
    }
    const std::size_t numPixels = columns * rows;
    const std::size_t outputBytes = numPixels * outputPixelBytes;
    if (!buffer && outputBytes > cse::memory::MaximumHunkPayloadBytes) {
        return fail("R_LoadTGA: texture exceeds hunk allocation capacity\n");
    }
    if (buffer && outputBytes > static_cast<std::size_t>(bufferSize)) {
        return fail("R_LoadTGA: texture exceeds output capacity\n");
    }
    const std::size_t remaining = static_cast<std::size_t>(fileLength) - HeaderBytes;
    const std::size_t idBytes = file[0];
    if (idBytes > remaining) {
        return fail("R_LoadTGA: file shorter than TGA image ID\n");
    }
    const byte* const imageData = file + HeaderBytes + idBytes;
    const std::size_t imageBytes = remaining - idBytes;
    const std::size_t inputPixelBytes = pixelSize / BitsPerByte;
    const auto decode = [&](byte* destination) -> bool {
        const byte* input = imageData;
        std::size_t inputRemaining = imageBytes;
        std::size_t pixel = 0;
        while (pixel < numPixels) {
            std::size_t packetPixels = numPixels - pixel;
            bool repeated = false;
            if (imageType == RleImageType) {
                if (!inputRemaining) {
                    return false;
                }
                const byte packetHeader = *input++;
                --inputRemaining;
                const std::size_t packetCount = 1 + (packetHeader & RleCountMask);
                if (packetPixels > packetCount) {
                    packetPixels = packetCount;
                }
                repeated = (packetHeader & RleRunFlag) != 0;
            }
            const std::size_t inputPixels = repeated ? 1 : packetPixels;
            if (inputPixels > inputRemaining / inputPixelBytes) {
                return false;
            }
            const std::size_t packetBytes = inputPixels * inputPixelBytes;
            inputRemaining -= packetBytes;
            if (!destination) {
                input += packetBytes;
                pixel += packetPixels;
                continue;
            }
            colorVec color{};
            for (std::size_t index = 0; index < packetPixels; ++index, ++pixel) {
                if (!repeated || !index) {
                    color.b = *input++;
                    color.g = *input++;
                    color.r = *input++;
                    color.a = pixelSize == RgbaBits ? *input++ : OpaqueAlpha;
                }
                const std::size_t row = rows - 1 - pixel / columns;
                byte* output = destination + (row * columns + pixel % columns) * outputPixelBytes;
                if (rgbaOutput) {
                    output[0] = static_cast<byte>(color.r);
                    output[1] = static_cast<byte>(color.g);
                    output[2] = static_cast<byte>(color.b);
                    output[3] = static_cast<byte>(color.a);
                } else {
                    const short packed = PutDitheredRGB(&color);
                    std::memcpy(output, &packed, sizeof(packed));
                }
            }
        }
        return true;
    };
    // Validate the complete packet stream before creating a persistent hunk allocation.
    if (!buffer) {
        if (!decode(NULL)) {
            return fail("R_LoadTGA: file shorter than length of texture\n");
        }
        buffer = static_cast<byte*>(Hunk_AllocName(static_cast<int>(outputBytes), "SKYBOX"));
    }
    if (!decode(buffer)) {
        return fail("R_LoadTGA: file shorter than length of texture\n");
    }
    COM_FreeFile(file);
    if (width) {
        *width = static_cast<int>(columns);
    }
    if (height) {
        *height = static_cast<int>(rows);
    }
    return buffer;
}

byte* R_LoadTGA(char* filename, byte* buffer, int bufferSize, int* width, int* height, qboolean output32bit) {
    constexpr int UnrestrictedDimension = 0;
    return R_LoadTGAImage(filename, buffer, bufferSize, width, height, output32bit, UnrestrictedDimension, true);
}

/*
=============
LoadTGA
=============
*/
qboolean LoadTGA(char* filename, byte* buffer, int bufferSize, int* width, int* height) {
    constexpr int UnrestrictedDimension = 0;
    return R_LoadTGAImage(filename, buffer, bufferSize, width, height, TRUE, UnrestrictedDimension) != NULL;
}

/*
==================
R_LoadSkys
==================
*/
void R_LoadSkys() {
    constexpr int SkyFaceDimension = 256;
    constexpr int SkyFaceBytes = SkyFaceDimension * SkyFaceDimension * sizeof(unsigned short);
    constexpr int SkyFaceCount = sizeof(suf) / sizeof(suf[0]);
    constexpr int SkyVertexCount = sizeof(gFakeVerts) / sizeof(gFakeVerts[0]);
    constexpr int RestartFaceSearch = -1;
    constexpr float SkyTextureScale = 0.0625f;
    constexpr float SkyPlaneDistance = 2048.0f;
    constexpr float SkyTextureOffset = SkyFaceDimension / 2.0f;
    if (!gLoadSky) {
        return;
    }

    qboolean printed = FALSE;
    for (int i = 0; i < SkyFaceCount; ++i) {
        char name[MAX_QPATH];
        snprintf(name, sizeof(name), "gfx/env/%s%s.tga", movevars.skyName, suf[i]);
        gSkies[i] = R_LoadTGAImage(name, NULL, 0, NULL, NULL, FALSE, SkyFaceDimension);
        if (!gSkies[i]) {
            Con_Printf("R_LoadSkys: Couldn't load %s\n", name);
            if (!i && Q_strcmp(movevars.skyName, "desert")) {
                Cvar_Set("movevars.skyname", "desert");
                i = RestartFaceSearch;
            }
            continue;
        }
        if (!printed) {
            Con_DPrintf("SKY:  ");
            printed = TRUE;
        }
        Con_DPrintf("%s%s, ", movevars.skyName, suf[i]);
        gFilteredSkies[i] = static_cast<pixel_t*>(Hunk_AllocName(SkyFaceBytes, "FILTEREDSKYBOX"));
    }
    if (printed) {
        Con_DPrintf("done\n");
    }

    for (int i = 0; i < SkyVertexCount; ++i) {
        gFakeEdges[i].v[0] = &gFakeVerts[i];
        gFakeEdges[i].v[1] = &gFakeVerts[(i + 1) % SkyVertexCount];
        gFakeEdges[i].pnext = i + 1 < SkyVertexCount ? &gFakeEdges[i + 1] : NULL;
    }
    gFakeTex.width = SkyFaceDimension;
    gFakeTex.height = SkyFaceDimension;
    Q_memset(gFakeSurface, 0, sizeof(gFakeSurface));
    Q_memset(gFakeTexInfo, 0, sizeof(gFakeTexInfo));
    for (int i = 0; i < SkyFaceCount; ++i) {
        msurface_t* surf = &gFakeSurface[i];
        mtexinfo_t* tex = &gFakeTexInfo[i];
        mplane_t* plane = &gFakePlanes[i];
        const int type = gFakePlaneType[i];
        surf->texturemins[0] = 0;
        surf->texturemins[1] = 0;
        surf->extents[0] = SkyFaceDimension;
        surf->extents[1] = SkyFaceDimension;
        surf->texinfo = tex;
        surf->samples = reinterpret_cast<color24*>(gSkies[skytexorder[i]]);
        surf->plane = plane;
        surf->flags = SURF_DRAWSKY;
        VectorClear(plane->normal);
        VectorClear(tex->vecs[0]);
        VectorClear(tex->vecs[1]);
        tex->texture = &gFakeTex;
        switch (type) {
        case 1:
        case -1:
            plane->normal[0] = 1;
            plane->type = 0;
            tex->vecs[0][1] = type < 0 ? SkyTextureScale : -SkyTextureScale;
            tex->vecs[1][2] = -SkyTextureScale;
            break;
        case 2:
        case -2:
            plane->normal[1] = 1;
            plane->type = 1;
            tex->vecs[0][0] = type < 0 ? -SkyTextureScale : SkyTextureScale;
            tex->vecs[1][2] = SkyTextureScale;
            break;
        case 3:
        case -3:
            plane->normal[2] = 1;
            plane->type = 2;
            tex->vecs[0][1] = -SkyTextureScale;
            tex->vecs[1][0] = type < 0 ? -SkyTextureScale : SkyTextureScale;
            break;
        }
        plane->dist = type < 0 ? -SkyPlaneDistance : SkyPlaneDistance;
        tex->vecs[0][3] = SkyTextureOffset;
        tex->vecs[1][3] = SkyTextureOffset;
        tex->mipadjust = 1;
    }
    gLoadSky = FALSE;
}

void DrawSkyPolygon( int nump, vec_t* vecs )
{
	vec3_t sum;
	VectorCopy( vec3_origin, sum );
	for ( int i = 0; i < nump; i++ )
		VectorAdd( sum, vecs + i * 3, sum );
	double x = fabs( (double)sum[0] );
	double y = fabs( (double)sum[1] );
	double z = fabs( (double)sum[2] );
	int axis;
	if ( x > y && x > z )
		axis = sum[0] < 0 ? 1 : 0;
	else if ( y > z && y > x )
		axis = sum[1] < 0 ? 3 : 2;
	else
		axis = sum[2] < 0 ? 5 : 4;

	for ( int i = 0; i < nump; i++, vecs += 3 )
	{
		int j = vec_to_st[axis][2];
		float depth = j > 0 ? vecs[j - 1] : -vecs[-j - 1];
		j = vec_to_st[axis][0];
		float s = ( j < 0 ? -vecs[-j - 1] : vecs[j - 1] ) / depth;
		j = vec_to_st[axis][1];
		float t = ( j < 0 ? -vecs[-j - 1] : vecs[j - 1] ) / depth;
		if ( s < skymins[0][axis] )
			skymins[0][axis] = s;
		if ( t < skymins[1][axis] )
			skymins[1][axis] = t;
		if ( s > skymaxs[0][axis] )
			skymaxs[0][axis] = s;
		if ( t > skymaxs[1][axis] )
			skymaxs[1][axis] = t;
	}
}

void ClipSkyPolygon( int nump, vec_t* vecs, int stage )
{
	if ( nump > MAX_CLIP_VERTS - 2 )
		Sys_Error( "ClipSkyPolygon: MAX_CLIP_VERTS" );
	if ( stage == 6 )
	{
		DrawSkyPolygon( nump, vecs );
		return;
	}

	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	qboolean front = FALSE, back = FALSE;
	for ( int i = 0; i < nump; i++ )
	{
		double d = DotProduct( vecs + 3 * i, skyclip[stage] );
		if ( d > 0.1 )
		{
			front = TRUE;
			sides[i] = SIDE_FRONT;
		}
		else if ( d < 0.1 )
		{
			back = TRUE;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = (float)d;
	}
	if ( !front || !back )
	{
		ClipSkyPolygon( nump, vecs, stage + 1 );
		return;
	}

	sides[nump] = sides[0];
	dists[nump] = dists[0];
	VectorCopy( vecs, vecs + 3 * nump );
	vec3_t newv[2][MAX_CLIP_VERTS];
	int count[2] = {};
	for ( int i = 0; i < nump; i++ )
	{
		float* v = vecs + i * 3;
		if ( sides[i] == SIDE_ON )
		{
			VectorCopy( v, newv[0][count[0]] );
			VectorCopy( v, newv[1][count[1]] );
			count[0]++;
			count[1]++;
			continue;
		}
		VectorCopy( v, newv[sides[i]][count[sides[i]]] );
		count[sides[i]]++;
		if ( sides[i + 1] == sides[i] || sides[i + 1] == SIDE_ON )
			continue;
		float frac = dists[i] / ( dists[i] - dists[i + 1] );
		for ( int j = 0; j < 3; j++ )
			newv[0][count[0]][j] = newv[1][count[1]][j] = v[j] + ( v[j + 3] - v[j] ) * frac;
		count[0]++;
		count[1]++;
	}
	ClipSkyPolygon( count[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( count[1], newv[1][0], stage + 1 );
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t* surf )
{
	gDrawSky = 1;
	if ( surf->numedges <= 0 )
		return;
	vec3_t verts[MAX_CLIP_VERTS];
	model_t* model = currententity->model;
	for ( int i = 0; i < surf->numedges; i++ )
	{
		int edge = model->surfedges[surf->firstedge + i];
		int vertex = edge > 0 ? model->edges[edge].v[0] : model->edges[-edge].v[1];
		VectorSubtract( r_pcurrentvertbase[vertex].position, r_origin, verts[i] );
	}
	ClipSkyPolygon( surf->numedges, verts[0], 0 );
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox()
{
	for ( int i = 0; i < 6; i++ )
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
	gDrawSky = 0;
}

void MakeSkyVec( float s, float t, int axis, vec_t* output )
{
#if defined( _WIN32 )
	if ( s < -1 )
		s = -1;
	else if ( s > 1 )
		s = 1;
	if ( t < -1 )
		t = -1;
	else if ( t > 1 )
		t = 1;
	vec3_t b = { (float)( s * 2048.0 ), (float)( t * 2048.0 ), 2048 };
#else
	if ( s < -1 )
		s = -1;
	else if ( !( s <= 1 ) )
		s = 1;
	if ( !( t >= -1 ) )
		t = -1;
	else if ( !( t <= 1 ) )
		t = 1;
	vec3_t b = { s * 2048, t * 2048, 2048 };
#endif
	for ( int i = 0; i < 3; i++ )
	{
		int j = st_to_vec[axis][i];
		output[i] = j < 0 ? -b[-j - 1] : b[j - 1];
	}
}

/*
==============
R_DrawSkyBox
==============
*/
void R_DrawSkyBox( int sortKey )
{
	if ( !gDrawSky )
		return;
	r_currentbkey = r_draworder.value ? 1 : sortKey + 1;
	r_clipflags = 15;
	vec3_t oldmodelorg;
	VectorCopy( modelorg, oldmodelorg );
	currententity = &gFakeEntity;
	VectorClear( gFakeEntity.angles );
	VectorCopy( r_origin, gFakeEntity.origin );
	VectorCopy( r_origin, r_entorigin );
	VectorSubtract( r_origin, gFakeEntity.origin, modelorg );
	VectorCopy( modelorg, r_worldmodelorg );
	R_RotateBmodel();
	insubmodel = TRUE;
	for ( int i = 0; i < 6; i++ )
	{
		if ( !( skymins[0][i] < skymaxs[0][i] ) || !( skymins[1][i] < skymaxs[1][i] ) )
			continue;
		MakeSkyVec( skymins[0][i], skymins[1][i], i, gFakeVerts[0].position );
		MakeSkyVec( skymins[0][i], skymaxs[1][i], i, gFakeVerts[1].position );
		MakeSkyVec( skymaxs[0][i], skymaxs[1][i], i, gFakeVerts[2].position );
		MakeSkyVec( skymaxs[0][i], skymins[1][i], i, gFakeVerts[3].position );
		R_RenderBmodelFace( gFakeEdges, &gFakeSurface[i] );
	}
	VectorCopy( oldmodelorg, modelorg );
	VectorCopy( base_vpn, vpn );
	VectorCopy( base_vright, vright );
	VectorCopy( base_vup, vup );
	R_TransformFrustum();
}

void R_InitSky()
{
	gLoadSky = 1;
}

void R_FilterSkyBox()
{
	float r = filterColorRed * filterBrightness;
	float g = filterColorGreen * filterBrightness;
	float b = filterColorBlue * filterBrightness;
	for ( int i = 0; i < 6; i++ )
	{
		if ( filterMode && gFakeSurface[i].samples )
		{
			unsigned short* input = (unsigned short*)gFakeSurface[i].samples;
			unsigned short* output = (unsigned short*)gFilteredSkies[i];
			for ( int j = 0; j < 256 * 256; j++ )
			{
				colorVec color;
				GetRGB( (short)input[j], &color );
#if defined( _WIN32 )
				color.r = (unsigned int)(long long)( color.r * r );
				color.g = (unsigned int)(long long)( color.g * g );
				color.b = (unsigned int)(long long)( color.b * b );
#else
				color.r = (unsigned int)(long long)( color.r * (double)r );
				color.g = (unsigned int)(long long)( color.g * (double)g );
				color.b = (unsigned int)(long long)( color.b * (double)b );
#endif
				output[j] = (unsigned short)PutDitheredRGB( &color );
			}
			gFakeSurface[i].samples = (color24*)output;
		}
		else
			gFakeSurface[i].samples = (color24*)gSkies[skytexorder[i]];
	}
}
