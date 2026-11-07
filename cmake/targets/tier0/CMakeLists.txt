# Generated from tier0/tier0.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(tier0 SHARED "tier0")
target_sources(tier0 PRIVATE
  "${CSE_SDK_DIR}/tier0/assert_dialog.cpp"
  "${CSE_SDK_DIR}/tier0/cpu.cpp"
  "${CSE_SDK_DIR}/tier0/cpu_posix.cpp"
  "${CSE_SDK_DIR}/tier0/dbg.cpp"
  "${CSE_SDK_DIR}/tier0/fasttimer.cpp"
  "${CSE_SDK_DIR}/tier0/mem.cpp"
  "${CSE_SDK_DIR}/tier0/mem_helpers.cpp"
  "${CSE_SDK_DIR}/tier0/memstd.cpp"
  "${CSE_SDK_DIR}/tier0/minidump.cpp"
  "${CSE_SDK_DIR}/tier0/platform_posix.cpp"
  "${CSE_SDK_DIR}/tier0/pme_posix.cpp"
  "${CSE_SDK_DIR}/tier0/pme.cpp"
  "${CSE_SDK_DIR}/tier0/testthread.cpp"
  "${CSE_SDK_DIR}/tier0/thread.cpp"
  "${CSE_SDK_DIR}/tier0/threadtools.cpp"
  "${CSE_SDK_DIR}/tier0/vcrmode_posix.cpp"
  "${CSE_SDK_DIR}/tier0/vprof.cpp"
  "${CSE_SDK_DIR}/tier0/validator.cpp"
  "${CSE_SDK_DIR}/tier0/valobject.cpp"
  "${CSE_SDK_DIR}/tier0/assert_dialog.rc"
)
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHa>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
set_property(TARGET tier0 PROPERTY CXX_STANDARD 14)
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:TIER0_DLL>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:NOMINMAX>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:WIN32_LEAN_AND_MEAN>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_include_directories(tier0 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(tier0 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(TARGET tier0 APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(tier0 PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(tier0 PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_sources(tier0 PRIVATE "${CSE_SDK_DIR}/tier0/tier0.def")
target_link_libraries(tier0 PRIVATE "$<$<CONFIG:Debug>:user32.lib>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHa>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(tier0 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
set_property(TARGET tier0 PROPERTY CXX_STANDARD 14)
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:TIER0_DLL>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:NOMINMAX>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:WIN32_LEAN_AND_MEAN>")
target_compile_definitions(tier0 PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_include_directories(tier0 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(tier0 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(TARGET tier0 APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(tier0 PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(tier0 PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_libraries(tier0 PRIVATE "$<$<CONFIG:Release>:user32.lib>")
cse_runtime_directory(tier0 "")
