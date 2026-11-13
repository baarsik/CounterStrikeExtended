# Inventory queries configure the production target modules and shared helpers.
# Reconfigure consumers when their authoritative CMake inputs change.
function(cse_fixture_inventory_dependencies root)
  file(GLOB_RECURSE inventory_cmake_inputs CONFIGURE_DEPENDS
    "${root}/cmake/*.cmake" "${root}/cmake/*/CMakeLists.txt")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${root}/CMakeLists.txt" "${root}/tools/cmake_inventory.py" ${inventory_cmake_inputs} ${ARGN})
endfunction()
