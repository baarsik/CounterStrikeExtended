#include "gl_model.h"
#if !defined(SW_RENDERER)
#include "glquake.h"
#endif
#include "common.h"
#include "zone.h"
#include "engine_studio.h"
#include "sys.h"
#include "model_loading.h"

#include <cstdio>

#if defined(SW_RENDERER)
extern byte texgammatable[256];
#endif

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel(model_t* mod, void* buffer, int fileBytes) {
    studiohdr_t header;
    int allocationBytes;
    if (!ModelLoading::Studio(buffer, fileBytes, header, allocationBytes)) {
        Sys_Error("Mod_LoadStudioModel: invalid or truncated model %s", mod->name);
        return;
    }

    byte* input = static_cast<byte*>(buffer);
    // Keep the historical writable-buffer fallback and hardware texture-index replacement.
    std::memcpy(input, &header, sizeof(header));
    mod->type = mod_studio;
    mod->flags = header.flags;
    Cache_Alloc(&mod->cache, allocationBytes, mod->name);
    byte* output = static_cast<byte*>(mod->cache.data);
    if (!output) {
        return;
    }

#if defined(SW_RENDERER)
    if (header.textureindex) {
        std::memcpy(output, input, header.texturedataindex);
        const byte* source = input + header.texturedataindex;
        byte* destination = output + header.texturedataindex;
        for (int i = 0; i < header.numtextures; ++i) {
            const std::size_t textureOffset = header.textureindex +
                static_cast<std::size_t>(i) * sizeof(mstudiotexture_t);
            mstudiotexture_t texture = ModelLoading::Read<mstudiotexture_t>(input + textureOffset);
            const int pixels = texture.width * texture.height;
            texture.index = static_cast<int>(destination - output);
            std::memcpy(output + textureOffset, &texture, sizeof(texture));
            std::memcpy(destination, source, pixels);
            source += pixels;
            destination += pixels;
            for (std::size_t color = 0; color < ModelLoading::PaletteEntries; ++color) {
                const word channels[ModelLoading::ExpandedPaletteChannels] = {
                    texgammatable[source[0]], texgammatable[source[1]], texgammatable[source[2]], 0
                };
                // Odd-sized textures legitimately leave this expanded word palette unaligned.
                std::memcpy(destination, channels, sizeof(channels));
                source += ModelLoading::PaletteChannels;
                destination += sizeof(channels);
            }
        }
    } else {
        std::memcpy(output, input, header.length);
    }
#else
    if (header.textureindex) {
        for (int i = 0; i < header.numtextures; ++i) {
            const std::size_t textureOffset = header.textureindex +
                static_cast<std::size_t>(i) * sizeof(mstudiotexture_t);
            mstudiotexture_t texture = ModelLoading::Read<mstudiotexture_t>(input + textureOffset);
            char name[256];
            snprintf(name, sizeof(name), "%s%.*s", mod->name, static_cast<int>(sizeof(texture.name)), texture.name);
            byte* pixels = input + texture.index;
            byte* palette = pixels + texture.width * texture.height;
            texture.index = GL_LoadTexture(name, GLT_STUDIO, texture.width, texture.height, pixels,
                !(texture.flags & STUDIO_NF_NOMIPS), (texture.flags & STUDIO_NF_MASKED) != 0, palette);
            std::memcpy(input + textureOffset, &texture, sizeof(texture));
        }
    }
    std::memcpy(output, input, allocationBytes);
#endif
}
