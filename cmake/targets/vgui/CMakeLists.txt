# Generated from utils/vgui/vgui.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(vgui SHARED "vgui")
target_sources(vgui PRIVATE
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/App.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Bitmap.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/BitmapTGA.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Border.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/BorderLayout.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/BorderPair.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/BuildGroup.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Button.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ButtonGroup.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/CheckButton.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Color.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/common/fileimage.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/common/vfontdata.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ConfigWizard.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Cursor.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Dar.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/DataInputStream.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/DefaultCursor.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Desktop.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/DesktopIcon.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/EditPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/EtchedBorder.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Exports.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/FileInputStream.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/FlowLayout.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/FocusNavGroup.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Font.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Frame.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/HeaderPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Image.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ImagePanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/IntLabel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Label.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Layout.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/LineBorder.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ListPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/LoweredBorder.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Menu.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/MenuItem.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/MenuSeparator.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/MessageBox.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/MiniApp.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Panel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/PopupMenu.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ProgressBar.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/RadioButton.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/RaisedBorder.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Scheme.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ScrollBar.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ScrollPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Slider.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/StackLayout.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/String.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/Surface.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/SurfaceBase.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TablePanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TabPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TaskBar.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TextEntry.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TextGrid.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TextImage.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TextPanel.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/ToggleButton.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/TreeFolder.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/vgui.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/win32/App.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/win32/Debug.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/win32/Font.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/win32/Surface.cpp"
  "${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/WizardPanel.cpp"
)
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET vgui PROPERTY CXX_STANDARD 14)
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Debug>:_USRDLL>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Debug>:VGUI_EXPORTS>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/include>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/common>")
set_property(TARGET vgui APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebug>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/EXPORT:VguiEmptyString,@395,DATA,EXPORTAS,??_C@_00A@?$AA@>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Debug>:/MANIFEST:NO>")
target_sources(vgui PRIVATE "${CSE_SDK_DIR}/utils/vgui/vgui.def")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Debug>:kernel32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Debug>:user32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Debug>:gdi32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Debug>:advapi32.lib>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(vgui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET vgui PROPERTY CXX_STANDARD 14)
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Release>:NDEBUG>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Release>:_USRDLL>")
target_compile_definitions(vgui PRIVATE "$<$<CONFIG:Release>:VGUI_EXPORTS>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/include>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui>")
target_include_directories(vgui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/lib-src/vgui/common>")
set_property(TARGET vgui APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreaded>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/EXPORT:VguiEmptyString,@395,DATA,EXPORTAS,??_C@_00A@?$AA@>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_options(vgui PRIVATE "$<$<CONFIG:Release>:/MANIFEST:NO>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Release>:kernel32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Release>:user32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Release>:gdi32.lib>")
target_link_libraries(vgui PRIVATE "$<$<CONFIG:Release>:advapi32.lib>")
cse_runtime_directory(vgui "")
cse_vgui_import_archive()
