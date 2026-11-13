#pragma once

#include "../../public/localization_service.h"

// Private provider helpers; no existing VGUI interface or control layout changes.
void ResetLocalizationFont(std::uint32_t font);
bool StageLocalizationFont(std::uint32_t font, const char* name, int tall, int weight,
    int blur, int scanlines, int flags);
