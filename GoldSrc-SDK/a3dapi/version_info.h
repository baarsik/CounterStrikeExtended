#pragma once

#include "platform_types.h"

#ifdef _WIN32
int		A3dCheckFileVersion( LPCSTR pszFilename, DWORD nBufferSize, LPVOID pBuffer );
int		A3dCheckSystemDriverVersion();

#endif
