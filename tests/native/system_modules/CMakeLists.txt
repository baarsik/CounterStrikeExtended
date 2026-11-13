cmake_minimum_required(VERSION 3.25)
project(native_system_modules LANGUAGES C CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
if(MSVC)
 # Match owning runtime policy for std::min and numeric_limits::max in fixture TUs.
 add_compile_definitions(NOMINMAX)
endif()
set(SDK "${CMAKE_CURRENT_LIST_DIR}/../../../src")
add_executable(interface_contract interface_contract.cpp)
target_include_directories(interface_contract PRIVATE "${SDK}/engine" "${SDK}/HLTV/Core/private" "${SDK}/HLTV/Core/private/common" "${SDK}" "${SDK}/common" "${SDK}/public" "${SDK}/public/tier1")
if(MSVC)
 target_compile_options(interface_contract PRIVATE /GR-)
else()
 target_compile_options(interface_contract PRIVATE -fno-rtti -Wno-write-strings)
endif()
add_test(NAME interface_contract COMMAND interface_contract)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
 # Legacy declaration compile/run check; native lifecycle cases require a 64-bit process.
 return()
endif()
foreach(owner engine HLTV/Core/private/common HLTV/DemoPlayer/private/common GameUI)
 string(REPLACE "/" "_" order "${owner}")
 add_executable(include_order_${order} include_order.cpp)
 target_include_directories(include_order_${order} PRIVATE "${SDK}/${owner}" "${SDK}/engine"
  "${SDK}/HLTV/Core/private" "${SDK}/HLTV/Core/private/common" "${SDK}/HLTV/common"
  "${SDK}" "${SDK}/common" "${SDK}/public" "${SDK}/public/tier1")
 if(MSVC)
  target_compile_options(include_order_${order} PRIVATE /GR-)
 else()
  target_compile_options(include_order_${order} PRIVATE -fno-rtti -Wno-write-strings)
 endif()
 add_test(NAME include_order_${order} COMMAND include_order_${order})
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CSE_SDK_DIR "${SDK}")
add_library(hltv_core SHARED)
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/targets/hltv_core/Sources.cmake")
target_include_directories(hltv_core PRIVATE "${SDK}/HLTV/Core/private" "${SDK}/HLTV/Core/private/public" "${SDK}/HLTV/Core/private/common" "${SDK}/HLTV/common" "${SDK}/launcher/public" "${SDK}")
add_library(demoplayer SHARED)
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/targets/demoplayer/Sources.cmake")
target_include_directories(demoplayer PRIVATE "${SDK}/HLTV/DemoPlayer/private" "${SDK}/HLTV/DemoPlayer/private/common" "${SDK}/HLTV/DemoPlayer/private/HLTV/common" "${SDK}/HLTV/DemoPlayer" "${SDK}/HLTV/DemoPlayer/private/public" "${SDK}" "${SDK}/common")
if(MSVC)
 # Match DemoPlayer's retained owner: its private mathlib uses M_PI.
 target_compile_definitions(demoplayer PRIVATE _USE_MATH_DEFINES)
endif()
foreach(target hltv_core demoplayer)
 if(MSVC)
  target_compile_options(${target} PRIVATE /GR-)
  target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
  target_link_libraries(${target} PRIVATE ws2_32)
 else()
  target_compile_options(${target} PRIVATE -fno-rtti -Wno-write-strings -Wno-format-security)
 endif()
endforeach()
if(NOT MSVC)
add_test(NAME frozen_controls COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/frozen_controls.py")
endif()

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/loader.cpp"
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_loader.py"
 "${SDK}/engine/SystemWrapper.cpp" "${CMAKE_CURRENT_BINARY_DIR}/loader.cpp"
 DEPENDS "${SDK}/engine/SystemWrapper.cpp" extract_loader.py)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cache_methods.cpp"
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_cache.py"
 "${SDK}/engine/DemoPlayerWrapper.cpp" "${CMAKE_CURRENT_BINARY_DIR}/cache_methods.cpp"
 DEPENDS "${SDK}/engine/DemoPlayerWrapper.cpp" extract_cache.py)
add_executable(lifecycle lifecycle.cpp "${CMAKE_CURRENT_BINARY_DIR}/cache_methods.cpp" "${CMAKE_CURRENT_BINARY_DIR}/loader.cpp"
 "${SDK}/common/native/BaseSystemModule.cpp" "${SDK}/common/ObjectList.cpp")
target_compile_definitions(lifecycle PRIVATE HLTV)
target_include_directories(lifecycle PRIVATE "${SDK}/HLTV/common" "${CMAKE_CURRENT_SOURCE_DIR}" "${SDK}" "${SDK}/public")
if(MSVC)
 target_compile_options(lifecycle PRIVATE /GR-)
 target_compile_definitions(lifecycle PRIVATE _CRT_SECURE_NO_WARNINGS)
 target_link_options(hltv_core PRIVATE /EXPORT:CreateInterface,@1)
else()
 target_compile_options(lifecycle PRIVATE -fno-rtti -Wno-write-strings)
endif()
if(CMAKE_CXX_FLAGS MATCHES "sanitize")
 add_test(NAME lifecycle COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> cycle0)
 foreach(cycle 1 2 3)
  add_test(NAME lifecycle_cycle${cycle} COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> cycle${cycle})
 endforeach()
else()
 add_test(NAME lifecycle COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer>)
endif()

foreach(mode engine server world command subscription)
 add_test(NAME failure_${mode} COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> ${mode})
endforeach()
set_tests_properties(lifecycle failure_engine failure_server failure_world failure_command PROPERTIES TIMEOUT 20)

add_test(NAME lifecycle_image_reload COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer>)
add_test(NAME module_name_copy COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> name-copy)

add_library(admission_provider SHARED admission_provider.cpp)
target_include_directories(admission_provider PRIVATE "${SDK}")
add_executable(admission_images admission_images.cpp)
target_include_directories(admission_images PRIVATE "${SDK}")
if(MSVC)
 target_compile_options(admission_provider PRIVATE /GR-)
 target_compile_options(admission_images PRIVATE /GR-)
else()
 target_compile_options(admission_provider PRIVATE -fno-rtti)
 target_compile_options(admission_images PRIVATE -fno-rtti)
endif()
add_test(NAME admission_images COMMAND admission_images $<TARGET_FILE:admission_provider>)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/console_loader.cpp"
 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_loader.py"
 "${SDK}/engine/SystemWrapper.cpp" "${CMAKE_CURRENT_BINARY_DIR}/console_loader.cpp" "${SDK}/HLTV/Console/System.cpp"
 DEPENDS "${SDK}/engine/SystemWrapper.cpp" "${SDK}/HLTV/Console/System.cpp" extract_loader.py)
add_executable(console_lifecycle lifecycle.cpp "${CMAKE_CURRENT_BINARY_DIR}/cache_methods.cpp"
 "${CMAKE_CURRENT_BINARY_DIR}/console_loader.cpp" "${SDK}/common/native/BaseSystemModule.cpp"
 "${SDK}/common/ObjectList.cpp")
target_include_directories(console_lifecycle PRIVATE "${SDK}/HLTV/common" "${CMAKE_CURRENT_SOURCE_DIR}" "${SDK}" "${SDK}/public")
if(MSVC)
 target_compile_options(console_lifecycle PRIVATE /GR-)
 target_compile_definitions(console_lifecycle PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
 target_compile_options(console_lifecycle PRIVATE -fno-rtti -Wno-write-strings)
endif()
add_test(NAME console_lifecycle COMMAND console_lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> cycle0)
target_compile_definitions(console_lifecycle PRIVATE CONSOLE_LOADER_FIXTURE)

if(APPLE)
 add_library(listener_allocator SHARED listener_allocator.cpp)
 # Preserve realloc(nullptr, size) in direct inline-storage checks; otherwise Clang
 # folds it to malloc and bypasses the realloc-only interposer. Provider flags stay unchanged.
 target_compile_options(lifecycle PRIVATE -fno-builtin-realloc)
 target_link_libraries(lifecycle PRIVATE listener_allocator)
 target_compile_definitions(lifecycle PRIVATE LISTENER_ALLOCATOR_INTERPOSITION)
 add_test(NAME failure_listener_allocation COMMAND lifecycle $<TARGET_FILE:hltv_core>
  $<TARGET_FILE:demoplayer> listener-allocation)
 set_tests_properties(failure_listener_allocation PROPERTIES TIMEOUT 20)
 if(CMAKE_CXX_FLAGS MATCHES "sanitize=address")
  execute_process(COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
   OUTPUT_VARIABLE clang_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  set(asan_runtime "${clang_resource_dir}/lib/darwin/libclang_rt.asan_osx_dynamic.dylib")
  if(NOT EXISTS "${asan_runtime}")
   message(FATAL_ERROR "AppleClang ASan runtime required for listener interposition: ${asan_runtime}")
  endif()
  # dyld applies these interposers in order: ASan must precede the test override.
  set_tests_properties(failure_listener_allocation PROPERTIES
   ENVIRONMENT "DYLD_INSERT_LIBRARIES=${asan_runtime}:$<TARGET_FILE:listener_allocator>")
  target_compile_definitions(lifecycle PRIVATE LISTENER_ALLOCATOR_ASAN)
 endif()
endif()

foreach(mode reactivation self-removal removal-frame publication
 running-order find-order frame-order instance-find instance-acquire)
 add_test(NAME registry_${mode} COMMAND lifecycle $<TARGET_FILE:hltv_core> $<TARGET_FILE:demoplayer> ${mode})
 add_test(NAME console_registry_${mode} COMMAND console_lifecycle $<TARGET_FILE:hltv_core>
  $<TARGET_FILE:demoplayer> ${mode})
endforeach()

if(NOT WIN32)
 add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/network_ownership.cpp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_network.py"
   "${SDK}/HLTV/Core/Network.cpp" "${CMAKE_CURRENT_BINARY_DIR}/network_ownership.cpp"
  DEPENDS "${SDK}/HLTV/Core/Network.cpp" extract_network.py)
 add_executable(fakeclient_cleanup fakeclient_cleanup.cpp "${CMAKE_CURRENT_BINARY_DIR}/network_ownership.cpp"
  "${SDK}/HLTV/Proxy/FakeClient.cpp" "${CMAKE_CURRENT_BINARY_DIR}/loader.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/cache_methods.cpp" "${SDK}/common/native/BaseSystemModule.cpp"
  "${SDK}/common/ObjectList.cpp")
 target_include_directories(fakeclient_cleanup PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${SDK}"
  "${SDK}/HLTV/common" "${SDK}/HLTV/Core/private" "${SDK}/public")
 target_compile_options(fakeclient_cleanup PRIVATE -fno-rtti -Wno-write-strings)
 target_sources(fakeclient_cleanup PRIVATE steam_fixture.cpp filesystem_fixture.cpp "${SDK}/HLTV/Proxy/Proxy.cpp" "${SDK}/HLTV/Proxy/ProxyClient.cpp"
  "${SDK}/HLTV/Proxy/Master.cpp" "${SDK}/HLTV/Proxy/Status.cpp" "${SDK}/HLTV/Proxy/DemoClient.cpp"
  "${SDK}/HLTV/Director/Director.cpp" "${SDK}/HLTV/common/BaseClient.cpp")
 target_include_directories(fakeclient_cleanup BEFORE PRIVATE "${SDK}/HLTV/Core/private")
 target_include_directories(fakeclient_cleanup PRIVATE "${SDK}/HLTV/Director")
 target_link_libraries(fakeclient_cleanup PRIVATE hltv_core)
 foreach(mode success failure proxy-stop proxy-remove network-remove fake-stop fake-parent)
  add_test(NAME fakeclient_cleanup_${mode} COMMAND fakeclient_cleanup $<TARGET_FILE:hltv_core> ${mode})
 endforeach()
endif()
