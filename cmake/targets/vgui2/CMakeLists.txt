# Generated from vgui2/vgui2.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(vgui2 SHARED "vgui2")
target_sources(vgui2 PRIVATE
  "${CSE_SDK_DIR}/vgui2/src/Bitmap.cpp"
  "${CSE_SDK_DIR}/vgui2/src/Border.cpp"
  "${CSE_SDK_DIR}/vgui2/src/fileimage.cpp"
  "${CSE_SDK_DIR}/vgui2/src/InputWin32.cpp"
  "${CSE_SDK_DIR}/vgui2/src/LocalizedStringTable.cpp"
  "${CSE_SDK_DIR}/vgui2/src/MemoryBitmap.cpp"
  "${CSE_SDK_DIR}/vgui2/src/MessageListener.cpp"
  "${CSE_SDK_DIR}/public/characterset.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/filesystem_helpers.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/interface.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/KeyValues.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/MemPool.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/UnicodeFileHelpers.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/UtlBuffer.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/utllinkedlist_inst.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/utlpriorityqueue_inst.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/utlrbtree.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/utlsymbol.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/utlvector_inst.cpp"
  "${CSE_SDK_DIR}/vgui2/src/Scheme.cpp"
  "${CSE_SDK_DIR}/vgui2/src/Surface.cpp"
  "${CSE_SDK_DIR}/vgui2/src/System.cpp"
  "${CSE_SDK_DIR}/vgui2/src/vgui.cpp"
  "${CSE_SDK_DIR}/vgui2/src/vgui_internal.cpp"
  "${CSE_SDK_DIR}/vgui2/src/vgui_surfacelib/vgui_key_translation.cpp"
  "${CSE_SDK_DIR}/vgui2/src/VGuiKeyValues.cpp"
  "${CSE_SDK_DIR}/vgui2/src/VPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/src/VPanelWrapper.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontAmalgam.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontManager.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/linuxfont.cpp"
)
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET vgui2 PROPERTY CXX_STANDARD 14)
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:_USRDLL>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_DEPRECATE>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/vgui_surfacelib>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(TARGET vgui2 APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/EXPORT:CreateInterface>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Debug>:shell32.lib>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Debug>:shlwapi.lib>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Debug>:winmm.lib>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
target_compile_options(vgui2 PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET vgui2 PROPERTY CXX_STANDARD 14)
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Release>:_USRDLL>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(vgui2 PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_DEPRECATE>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/vgui_surfacelib>")
target_include_directories(vgui2 PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(TARGET vgui2 APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/EXPORT:CreateInterface>")
target_link_options(vgui2 PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Release>:shell32.lib>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Release>:shlwapi.lib>")
target_link_libraries(vgui2 PRIVATE "$<$<CONFIG:Release>:winmm.lib>")
cse_runtime_directory(vgui2 "")
