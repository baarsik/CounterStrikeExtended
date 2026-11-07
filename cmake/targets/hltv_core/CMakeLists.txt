# Generated from HLTV/Core/Core.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(hltv_core SHARED "Core")
target_sources(hltv_core PRIVATE
  "${CSE_SDK_DIR}/HLTV/Core/BSPModel.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Core.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/delta.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/NetSocket.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Network.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Server.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/World.cpp"
  "${CSE_SDK_DIR}/HLTV/common/BitBuffer.cpp"
  "${CSE_SDK_DIR}/HLTV/common/byteorder.cpp"
  "${CSE_SDK_DIR}/HLTV/common/common.cpp"
  "${CSE_SDK_DIR}/HLTV/common/DemoFile.cpp"
  "${CSE_SDK_DIR}/HLTV/common/DirectorCmd.cpp"
  "${CSE_SDK_DIR}/HLTV/common/InfoString.cpp"
  "${CSE_SDK_DIR}/HLTV/common/mathlib.cpp"
  "${CSE_SDK_DIR}/HLTV/common/md5.cpp"
  "${CSE_SDK_DIR}/HLTV/common/munge.cpp"
  "${CSE_SDK_DIR}/HLTV/common/NetAddress.cpp"
  "${CSE_SDK_DIR}/HLTV/common/NetChannel.cpp"
  "${CSE_SDK_DIR}/HLTV/common/random.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/common/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/common/ObjectDictionary.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/public/interface.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/common/TokenLine.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/blocksort.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/compress.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/crctable.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/decompress.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/huffman.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/randtable.c"
)
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/RTC1>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET hltv_core PROPERTY CXX_STANDARD 17)
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:_USRDLL>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:NOMINMAX>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Debug>:_WINSOCK_DEPRECATED_NO_WARNINGS>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Core/private>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Core/private/public>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Core/private/common>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/common>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/launcher/public>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(TARGET hltv_core APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/EXPORT:CreateInterface,@1>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_link_libraries(hltv_core PRIVATE "$<$<CONFIG:Debug>:ws2_32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_DEBUG>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:WIN32>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_WINDOWS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_USRDLL>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:NOMINMAX>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:_WINSOCK_DEPRECATED_NO_WARNINGS>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET hltv_core PROPERTY CXX_STANDARD 17)
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:NDEBUG>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:_USRDLL>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:NOMINMAX>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hltv_core PRIVATE "$<$<CONFIG:Release>:_WINSOCK_DEPRECATED_NO_WARNINGS>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Core/private>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Core/private/public>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Core/private/common>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/common>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/launcher/public>")
target_include_directories(hltv_core PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(TARGET hltv_core APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(hltv_core PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/EXPORT:CreateInterface,@1>")
target_link_options(hltv_core PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_libraries(hltv_core PRIVATE "$<$<CONFIG:Release>:ws2_32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:NDEBUG>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:WIN32>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:_WINDOWS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:_USRDLL>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:NOMINMAX>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c" APPEND PROPERTY COMPILE_DEFINITIONS "$<$<CONFIG:Release>:_WINSOCK_DEPRECATED_NO_WARNINGS>")
cse_runtime_directory(hltv_core "")
