#include "cmodel.h"
#include "common.h"
#include "gl_model.h"
#include "mem.h"
#include "conprint.h"
#include "server.h"
#include "sys.h"
#include "model_visibility.h"

#include <algorithm>
#include <cstring>
#include <limits>

static byte* gPAS;
static byte* gPVS;
static int gPVSRowBytes;
static int gPVSLeafCount;
static const model_t* gPVSModel;
static byte mod_novis[ModelVisibility::MaximumRowBytes];

byte* CM_LeafPVS(int leafnum) {
    if (!gPVS) {
        return mod_novis;
    }
    if (leafnum < 0 || leafnum >= gPVSLeafCount) {
        Sys_Error("CM_LeafPVS: invalid leaf index");
        return mod_novis;
    }
    return gPVS + static_cast<std::size_t>(leafnum) * gPVSRowBytes;
}

byte* CM_LeafPAS(int leafnum) {
    if (!gPAS) {
        return mod_novis;
    }
    if (leafnum < 0 || leafnum >= gPVSLeafCount) {
        Sys_Error("CM_LeafPAS: invalid leaf index");
        return mod_novis;
    }
    return gPAS + static_cast<std::size_t>(leafnum) * gPVSRowBytes;
}

void Mod_Init() {
#if defined(SW_RENDERER)
    SW_Mod_Init();
#endif
    std::memset(mod_novis, ModelVisibility::AllVisible, sizeof(mod_novis));
}

bool CM_DecompressPVS(const byte* input, std::size_t inputBytes, byte* output, std::size_t byteCount) {
    if (byteCount == 0) {
        return true;
    }
    if (!output || byteCount > ModelVisibility::MaximumRowBytes) {
        return false;
    }
    if (!input) {
        std::memset(output, ModelVisibility::AllVisible, byteCount);
        return true;
    }

    std::size_t source = 0;
    std::size_t destination = 0;
    while (destination < byteCount) {
        if (source == inputBytes) {
            return false;
        }
        const byte value = input[source++];
        if (value != 0) {
            output[destination++] = value;
            continue;
        }
        if (source == inputBytes || input[source] == 0) {
            return false;
        }
        // BSP encoders may pad the last zero run beyond the logical visibility row.
        const std::size_t run = std::min<std::size_t>(input[source++], byteCount - destination);
        std::memset(output + destination, 0, run);
        destination += run;
    }
    return true;
}

static bool Mod_DecodeVisibility(const byte* input, model_t* model, byte* output, std::size_t rowBytes) {
    const ModelVisibilityInfo info = Mod_GetVisibilityInfo(model);
    std::size_t inputBytes;
    return ModelVisibility::SourceBytes(model, input, info, inputBytes) &&
        CM_DecompressPVS(input, inputBytes, output, rowBytes);
}

byte* Mod_DecompressVis(byte* input, model_t* model) {
    static byte decompressed[ModelVisibility::MaximumRowBytes];
    std::size_t rowBytes;
    const ModelVisibilityInfo info = Mod_GetVisibilityInfo(model);
    if (!model || !ModelVisibility::RowBytes(model->numleafs, rowBytes) ||
        info.leafCount <= model->numleafs ||
        !Mod_DecodeVisibility(input, model, decompressed, rowBytes)) {
        std::memset(decompressed, 0, sizeof(decompressed));
        Sys_Error("Mod_DecompressVis: invalid visibility row");
        return mod_novis;
    }
    return input ? decompressed : mod_novis;
}

byte* Mod_LeafPVS(mleaf_t* leaf, model_t* model) {
    int index;
    if (!model || !ModelVisibility::LeafIndex(model, leaf, Mod_GetVisibilityInfo(model), index)) {
        Sys_Error("Mod_LeafPVS: invalid leaf");
        return mod_novis;
    }
    if (index == 0) {
        return mod_novis;
    }
    if (gPVS && gPVSModel == model && index < gPVSLeafCount) {
        return CM_LeafPVS(index);
    }
    return Mod_DecompressVis(leaf->compressed_vis, model);
}

void CM_FreePAS() {
    if (gPAS) {
        Mem_Free(gPAS);
    }
    if (gPVS) {
        Mem_Free(gPVS);
    }
    gPVS = nullptr;
    gPAS = nullptr;
    gPVSRowBytes = 0;
    gPVSLeafCount = 0;
    gPVSModel = nullptr;
}

void CM_InvalidatePAS(const model_t* model) {
    if (gPVSModel == model) {
        CM_FreePAS();
    }
}

void CM_CalcPAS(model_t* model) {
    Con_DPrintf("Building PAS...\n");
    CM_FreePAS();

    std::size_t rowBytes;
    const ModelVisibilityInfo info = Mod_GetVisibilityInfo(model);
    if (!model || !model->leafs || !ModelVisibility::RowBytes(model->numleafs, rowBytes) ||
        info.leafCount <= model->numleafs) {
        Sys_Error("CM_CalcPAS: invalid leaf count");
        return;
    }
    if (rowBytes == 0) {
        return;
    }

    constexpr std::size_t RowAlignment = sizeof(unsigned int);
    const std::size_t alignedRowBytes = (rowBytes + RowAlignment - 1) & ~(RowAlignment - 1);
    const int count = model->numleafs + 1;
    if (static_cast<std::size_t>(count) > std::numeric_limits<std::size_t>::max() / alignedRowBytes) {
        Sys_Error("CM_CalcPAS: visibility allocation overflow");
        return;
    }
    const int rowwords = static_cast<int>(alignedRowBytes / RowAlignment);
    gPVSRowBytes = static_cast<int>(alignedRowBytes);
    gPVSLeafCount = count;
    gPVSModel = model;
    gPVS = static_cast<byte*>(Mem_Calloc(count, alignedRowBytes));
    if (!gPVS) {
        CM_FreePAS();
        Sys_Error("CM_CalcPAS: visibility allocation failed");
        return;
    }

    int vcount = 0;
    for (int i = 0; i < count; ++i) {
        byte* scan = CM_LeafPVS(i);
        if (!Mod_DecodeVisibility(model->leafs[i].compressed_vis, model, scan, rowBytes)) {
            CM_FreePAS();
            Sys_Error("CM_CalcPAS: invalid visibility row");
            return;
        }
        if (i != 0) {
            for (int j = 0; j < model->numleafs; ++j) {
                if (scan[j / ModelVisibility::BitsPerByte] & (1 << (j % ModelVisibility::BitsPerByte))) {
                    ++vcount;
                }
            }
        }
    }

    gPAS = static_cast<byte*>(Mem_Calloc(count, alignedRowBytes));
    if (!gPAS) {
        CM_FreePAS();
        Sys_Error("CM_CalcPAS: audibility allocation failed");
        return;
    }
    int acount = 0;
    for (int i = 0; i < count; ++i) {
        const byte* scan = CM_LeafPVS(i);
        unsigned int* destination = reinterpret_cast<unsigned int*>(CM_LeafPAS(i));
        std::memcpy(destination, scan, alignedRowBytes);
        for (std::size_t j = 0; j < rowBytes; ++j) {
            const int bitbyte = scan[j];
            for (int k = 0; k < ModelVisibility::BitsPerByte; ++k) {
                if (!(bitbyte & (1 << k))) {
                    continue;
                }
                const std::size_t index = j * ModelVisibility::BitsPerByte + k + 1;
                if (index >= static_cast<std::size_t>(count)) {
                    continue;
                }
                const unsigned int* source = reinterpret_cast<const unsigned int*>(CM_LeafPVS(static_cast<int>(index)));
                for (int word = 0; word < rowwords; ++word) {
                    destination[word] |= source[word];
                }
            }
        }
        if (i != 0) {
            for (int j = 0; j < model->numleafs; ++j) {
                if (reinterpret_cast<byte*>(destination)[j / ModelVisibility::BitsPerByte] &
                    (1 << (j % ModelVisibility::BitsPerByte))) {
                    ++acount;
                }
            }
        }
    }
    Con_DPrintf("Average leaves visible / audible / total: %i / %i / %i\n", vcount / count, acount / count, count);
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible( mnode_t* node, byte* visbits, int* firstVisibleLeaf )
{
	int leafnum;
	mleaf_t* leaf;

	while ( node && node->contents != CONTENTS_SOLID )
	{
		if ( node->contents < 0 )
		{
			leaf = (mleaf_t*)node;
			leafnum = (int)( leaf - sv.worldmodel->leafs ) - 1;

			if ( !( visbits[leafnum >> 3] & ( 1 << ( leafnum & 7 ) ) ) )
				return FALSE;

			if ( firstVisibleLeaf )
				*firstVisibleLeaf = leafnum;
			return TRUE;
		}

		if ( CM_HeadnodeVisible( node->children[0], visbits, firstVisibleLeaf ) )
			return TRUE;

		node = node->children[1];
	}

	return FALSE;
}
