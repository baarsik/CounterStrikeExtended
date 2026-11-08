include_guard(GLOBAL)
# This file is included before project(), when deployment flags are initialized.
# An explicitly selected cross target always wins over the builder's platform.
if(CMAKE_SYSTEM_NAME STREQUAL Darwin OR (NOT CMAKE_SYSTEM_NAME AND CMAKE_HOST_APPLE))
  if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum supported macOS" FORCE)
  elseif(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 14.0)
    message(FATAL_ERROR "Native dependencies and fixtures require macOS 14 or newer")
  endif()
endif()
