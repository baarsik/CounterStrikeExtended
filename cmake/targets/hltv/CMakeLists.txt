# Generated from projects/vs2026/HLTV.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(hltv EXECUTABLE "hltv")
target_sources(hltv PRIVATE
  "${CSE_SDK_DIR}/HLTV/Console/System.cpp"
  "${CSE_SDK_DIR}/HLTV/Console/TextConsole.cpp"
  "${CSE_SDK_DIR}/HLTV/Console/TextConsoleWin32.cpp"
  "${CSE_SDK_DIR}/HLTV/common/common.cpp"
  "${CSE_SDK_DIR}/HLTV/common/random.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/common/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/common/TokenLine.cpp"
  "${CSE_SDK_DIR}/engine/interface.cpp"
  "${CSE_SDK_DIR}/engine/KeyValues.cpp"
  "${CSE_SDK_DIR}/engine/utlbuffer.cpp"
  "${CSE_SDK_DIR}/engine/utlsymbol.cpp"
  "${CSE_SDK_DIR}/engine/MemPool.cpp"
  "${CSE_SDK_DIR}/engine/controls.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/controls.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Panel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextEntry.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Menu.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBar.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MenuItem.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBarSlider.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Label.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Button.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Image.cpp"
  "${CSE_SDK_DIR}/HLTV/Console/hltv.rc"
)
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/RTC1>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
set_property(TARGET hltv PROPERTY CXX_STANDARD 17)
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:HLTV_CONSOLE>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:CLIENT_DLL>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:NOMINMAX>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Core/private>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Core/private/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/Console>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(TARGET hltv APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/OPT:REF>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/LARGEADDRESSAWARE>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/EXPORT:CreateInterface,@1>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(hltv PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
set_property(TARGET hltv PROPERTY CXX_STANDARD 17)
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:HLTV_CONSOLE>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:CLIENT_DLL>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:NOMINMAX>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hltv PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Core/private>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Core/private/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/Console>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(hltv PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(TARGET hltv APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(hltv PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/LARGEADDRESSAWARE>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/EXPORT:CreateInterface,@1>")
target_link_options(hltv PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
cse_runtime_directory(hltv "")
