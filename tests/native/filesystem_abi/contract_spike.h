#pragma once

// Compile the production family against each real retained header view.
#if defined(CSE_FS_PROVIDER_VIEW)
#include "basefilesystem.h"
#elif defined(CSE_FS_LAUNCHER_VIEW)
#include "launcher/public/filesystem.h"
#elif defined(CSE_FS_VGUI_VIEW)
#include "vgui2/src/public/FileSystem.h"
#else
#include "public/FileSystem.h"
#endif
#include "public/cse_filesystem_abi.h"
namespace filesystem_spike = cse::filesystem_abi;
