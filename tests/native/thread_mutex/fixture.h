#pragma once

#include <cstdint>

#if defined(_WIN32)
#include "selected_header.h"
#else
#include "storage_model.h"
#endif

void SetFixtureThreadId(std::uint32_t value);
