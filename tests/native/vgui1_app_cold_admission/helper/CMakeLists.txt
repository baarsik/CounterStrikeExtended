cmake_minimum_required(VERSION 3.25)
project(CseAppColdHelperControls LANGUAGES CXX)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
include("${root}/cmake/VGui1AppColdMetadata.cmake")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
add_library(metadata_offer SHARED anchor.cpp)
add_library(metadata_requirement SHARED anchor.cpp)
target_link_libraries(metadata_requirement PRIVATE metadata_offer)
set_target_properties(metadata_offer PROPERTIES BUILD_WITH_INSTALL_NAME_DIR YES
    INSTALL_NAME_DIR "${CMAKE_CURRENT_BINARY_DIR}")
foreach(target metadata_offer metadata_requirement)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-fast-math)
endforeach()
cse_add_vgui1_app_cold_metadata(metadata_offer offer "${CSE_APP_COLD_CONTEXTS}"
    "${CMAKE_CURRENT_BINARY_DIR}/offer-evidence")
cse_add_vgui1_app_cold_metadata(metadata_requirement requirement "${CSE_APP_COLD_CONTEXTS}"
    "${CMAKE_CURRENT_BINARY_DIR}/requirement-evidence")
