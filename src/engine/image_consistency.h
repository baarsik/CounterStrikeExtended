#pragma once

#include <string>

class EngineSurface;
class IEngineSurface;

namespace cse {
namespace engine_image {

struct FilePolicy {
    std::string targaPath;
    std::string webpPath;
    bool targaChecked = false;
    bool webpChecked = false;

    bool IsProtected() const {
        return targaChecked || webpChecked;
    }
};

// These are engine-private helpers; no client interface or texture record changes.
FilePolicy GetFilePolicy(const char* filename);
void RememberFileTexture(EngineSurface* owner, int id, const char* filename, int hardwareFilter);
void ForgetFileTexture(IEngineSurface* owner, int id);
void ForgetFileTextures(EngineSurface* owner);
void RefreshFileTextures(EngineSurface* owner);
void RefreshFileTextures();

}
}
