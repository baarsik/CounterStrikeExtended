if(NOT ((WIN32 AND MSVC AND CSE_TARGET_ARCH MATCHES "^(x86|x64|arm64)$")
    OR (APPLE AND CSE_TARGET_ARCH STREQUAL "arm64")))
  message(FATAL_ERROR "ReGameDLL supports Windows x86/x64/ARM64 and macOS ARM64")
endif()
if(WIN32 AND NOT CSE_BUILD_INVENTORY)
  enable_language(RC)
endif()

find_package(Git REQUIRED)
set(submodule "${CSE_SDK_DIR}/external/regamedll")
if(NOT EXISTS "${submodule}/regamedll/CMakeLists.txt")
  message(FATAL_ERROR "Initialize the ReGameDLL submodule with git submodule update --init src/external/regamedll")
endif()

execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" rev-parse HEAD
  OUTPUT_VARIABLE regamedll_commit OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT regamedll_commit STREQUAL "1f16a7f25c14225931f8df1abc51f68d18ded839")
  message(FATAL_ERROR "The CSE adapter requires ReGameDLL commit 1f16a7f25c14225931f8df1abc51f68d18ded839")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ReGameDLLContract.cmake")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" show "${regamedll_commit}:regamedll/engine/eiface.h"
  OUTPUT_VARIABLE regamedll_eiface COMMAND_ERROR_IS_FATAL ANY)
cse_check_regamedll_contract("${regamedll_eiface}")

set(integration "${CSE_SDK_DIR}/cstrike/regamedll")
set(source "${CMAKE_CURRENT_BINARY_DIR}/regamedll-source")
set(patch "${integration}/cse.patch")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" rev-parse --path-format=absolute --git-path HEAD
  OUTPUT_VARIABLE submodule_head OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${submodule_head}" "${patch}")
file(SHA256 "${patch}" patch_checksum)
set(revision "${regamedll_commit} ${patch_checksum}")
set(stamp "${source}/cse-revision")
set(staged_revision "")
if(EXISTS "${stamp}")
  file(READ "${stamp}" staged_revision)
endif()
if(NOT staged_revision STREQUAL revision)
  file(REMOVE_RECURSE "${source}")
  file(MAKE_DIRECTORY "${source}")
  set(archive "${CMAKE_CURRENT_BINARY_DIR}/regamedll-source.tar")
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" archive --output "${archive}" "${regamedll_commit}"
    COMMAND_ERROR_IS_FATAL ANY)
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${source}")
  file(REMOVE "${archive}")
  # Keep git apply from filtering paths against the enclosing CSE checkout.
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_BINARY_DIR}"
    "${GIT_EXECUTABLE}" apply --ignore-space-change --whitespace=nowarn "${patch}"
    WORKING_DIRECTORY "${source}" COMMAND_ERROR_IS_FATAL ANY)
  file(WRITE "${stamp}" "${revision}")
endif()

set(CSE_REGAMEDLL_INTEGRATION "${integration}")
set(CSE_REGAMEDLL_STAGED_SOURCE_ROOT "${source}")
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${submodule}" show -s --format=%cs "${regamedll_commit}"
  OUTPUT_VARIABLE regamedll_commit_date OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
configure_file("${integration}/appversion.h.in" "${source}/regamedll/version/appversion.h" @ONLY)
include("${CMAKE_CURRENT_LIST_DIR}/ReGameDLLTarget.cmake")
set_target_properties(cs_server PROPERTIES
  CSE_CS_SERVER_BACKEND regamedll
  CSE_REGAMEDLL_STAGED_SOURCE_ROOT "${source}"
  CSE_REGAMEDLL_ORIGINAL_SOURCE_ROOT "${submodule}"
  CSE_REGAMEDLL_PATCH "${patch}"
  CSE_REGAMEDLL_REVISION "${regamedll_commit}")
