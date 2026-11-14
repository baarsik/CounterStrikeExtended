#include "image_consistency.h"
#include "engine_image.h"
#include "client.h"
#include "eiface.h"
#include "vgui_EngineSurface.h"

#include <map>
#include <utility>
#include <vector>

namespace cse {
namespace engine_image {
namespace {

std::string NormalizePath(const char* filename, bool foldCase) {
    std::string path(filename ? filename : "");
    for (char& character : path) {
        if (character == '\\') {
            character = '/';
        } else if (foldCase && character >= 'A' && character <= 'Z') {
            character += 'a' - 'A';
        }
    }
    while (path.compare(0, 2, "./") == 0) {
        path.erase(0, 2);
    }
    return path;
}

bool FindCheckedPath(std::string& path) {
    const std::string normalized = NormalizePath(path.c_str(), true);
    for (int index = 0; index < cl.num_consistency; ++index) {
        const consistency_t& entry = cl.consistency_list[index];
        if (!entry.issound && entry.check_type == force_exactfile && entry.filename
            && NormalizePath(entry.filename, true) == normalized) {
            // The matching wire filename is also the file whose bytes are hashed.
            path = NormalizePath(entry.filename, false);
            return true;
        }
    }
    return false;
}

struct FileTexture {
    std::string filename;
    int hardwareFilter;
    bool wasProtected;
};

using OwnerTextures = std::map<int, FileTexture>;
using FileTextureOwners = std::map<IEngineSurface*, OwnerTextures>;

FileTextureOwners& FileTextures() {
    // Interface singletons can outlive ordinary static destructors. Owners remove
    // all their entries on destruction; retain only this registry for process life.
    static FileTextureOwners* textures = new FileTextureOwners;
    return *textures;
}

}

FilePolicy GetFilePolicy(const char* filename) {
    FilePolicy policy;
    if (!filename || !*filename) {
        return policy;
    }
    policy.targaPath = TargaPath(filename);
    if (image::HasWebPExtension(filename)) {
        policy.targaPath.resize(policy.targaPath.find_last_of('.'));
        policy.targaPath += ".tga";
    }
    policy.webpPath = image::WebPPath(filename);
    const std::string normalized = NormalizePath(filename, true);
    if (HasExtension(filename) && !image::HasWebPExtension(filename)
        && normalized.substr(normalized.find_last_of('.')) != ".tga") {
        // This policy governs TGA/WebP alternatives, not literal BMP loads.
        return policy;
    }
    policy.targaChecked = FindCheckedPath(policy.targaPath);
    policy.webpChecked = FindCheckedPath(policy.webpPath);
    return policy;
}

void RememberFileTexture(EngineSurface* owner, int id, const char* filename, int hardwareFilter) {
    if (owner && id > 0 && filename && *filename) {
        FileTextures()[owner][id] = {filename, hardwareFilter, GetFilePolicy(filename).IsProtected()};
    }
}

void ForgetFileTexture(IEngineSurface* owner, int id) {
    auto& owners = FileTextures();
    const auto found = owners.find(owner);
    if (found != owners.end()) {
        found->second.erase(id);
        if (found->second.empty()) {
            owners.erase(found);
        }
    }
}

void ForgetFileTextures(EngineSurface* owner) {
    FileTextures().erase(owner);
}

void RefreshFileTextures(EngineSurface* owner) {
    const auto found = FileTextures().find(owner);
    if (found == FileTextures().end()) {
        return;
    }
    std::vector<std::pair<int, FileTexture>> pending;
    for (const auto& texture : found->second) {
        if (texture.second.wasProtected || GetFilePolicy(texture.second.filename.c_str()).IsProtected()) {
            pending.push_back(texture);
        }
    }
    // Uploads retire provenance before the file loader restores it. Work from an
    // owned snapshot so replacing the last entry cannot invalidate iteration.
    for (const auto& texture : pending) {
        owner->drawSetTextureFile(
            texture.first, texture.second.filename.c_str(), texture.second.hardwareFilter, true);
    }
}

void RefreshFileTextures() {
    std::vector<EngineSurface*> owners;
    for (const auto& owner : FileTextures()) {
        owners.push_back(static_cast<EngineSurface*>(owner.first));
    }
    for (EngineSurface* owner : owners) {
        owner->refreshFileTextures();
    }
}

}
}
