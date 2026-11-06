# Generated from projects/vs2026/hlds.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(hlds EXECUTABLE "hlds")
target_sources(hlds PRIVATE
  "${CSE_SDK_DIR}/dedicated/sys_ded.cpp"
  "${CSE_SDK_DIR}/dedicated/CreateMultiplayerGameServerPage.cpp"
  "${CSE_SDK_DIR}/dedicated/AdminServer.cpp"
  "${CSE_SDK_DIR}/dedicated/console/conproc.cpp"
  "${CSE_SDK_DIR}/common/textconsole.cpp"
  "${CSE_SDK_DIR}/common/TextConsoleWin32.cpp"
  "${CSE_SDK_DIR}/common/commandline.cpp"
  "${CSE_SDK_DIR}/common/netapi.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/engine/interface.cpp"
  "${CSE_SDK_DIR}/engine/mem.c"
  "${CSE_SDK_DIR}/engine/KeyValues.cpp"
  "${CSE_SDK_DIR}/engine/utlbuffer.cpp"
  "${CSE_SDK_DIR}/engine/utlsymbol.cpp"
  "${CSE_SDK_DIR}/engine/MemPool.cpp"
  "${CSE_SDK_DIR}/engine/controls.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/controls.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/AnimatingImagePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BitmapImagePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BuildGroup.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BuildModeDialog.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Button.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/CheckButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ComboBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Divider.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/EditablePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/FocusNavGroup.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Frame.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/HTML.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Image.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ImageList.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ImagePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Label.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Menu.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MenuButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MenuItem.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MessageBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Panel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PanelListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ProgressBar.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ProgressBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PropertyPage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PropertySheet.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/QueryBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/RadioButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/RichText.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBar.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBarSlider.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextEntry.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ToggleButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/URLLabel.cpp"
  "${CSE_SDK_DIR}/dedicated/hlds.rc"
)
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/external:I${CSE_SDK_DIR}/external/SDL2>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze:external->")
set_property(TARGET hlds PROPERTY CXX_STANDARD 17)
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:CLIENT_DLL>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:HLDS_LAUNCHER>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/dedicated>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(TARGET hlds APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/OPT:REF>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/LARGEADDRESSAWARE>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/EXPORT:CreateInterface,@1>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/MANIFEST:NO>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Debug>:/MAP:${CMAKE_BINARY_DIR}/symbols/Debug/hlds.map>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Debug>:ws2_32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Debug>:winmm.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Debug>:user32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Debug>:advapi32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Debug>:shell32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mem.c" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/TP>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/external:I${CSE_SDK_DIR}/external/SDL2>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze>")
target_compile_options(hlds PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze:external->")
set_property(TARGET hlds PROPERTY CXX_STANDARD 17)
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:CLIENT_DLL>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:HLDS_LAUNCHER>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hlds PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/dedicated>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(hlds PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(TARGET hlds APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/LARGEADDRESSAWARE>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/EXPORT:CreateInterface,@1>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/MANIFEST:NO>")
target_link_options(hlds PRIVATE "$<$<CONFIG:Release>:/MAP:${CMAKE_BINARY_DIR}/symbols/Release/hlds.map>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Release>:ws2_32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Release>:winmm.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Release>:user32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Release>:advapi32.lib>")
target_link_libraries(hlds PRIVATE "$<$<CONFIG:Release>:shell32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mem.c" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/TP>")
cse_runtime_directory(hlds "")
