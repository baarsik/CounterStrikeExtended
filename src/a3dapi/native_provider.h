#pragma once
#include "native_device_init.h"

// Each factory returns one owned canonical IUnknown. The leaf never constructs
// a facade; only the facade attaches an initialized leaf to resource ownership.
HRESULT A3dCreateNativeProvider(IUnknown** provider);
HRESULT A3dCreateNativeLeaf(IUnknown** leaf);

bool A3dIsNativeProvider(IUnknown* object) noexcept;
void A3dShutdownNativeLeaf(IUnknown* object) noexcept;
