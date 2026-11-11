# Generated from projects/vs2026/hlds.vcxproj; see tools/translate_msbuild.py.
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
  "${CSE_SDK_DIR}/common/TextEntryClipboard.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ToggleButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/URLLabel.cpp"
)
if(WIN32)
  target_sources(hlds PRIVATE
    "${CSE_SDK_DIR}/dedicated/hlds.rc"
  )
endif()
if(APPLE)
  set_property(SOURCE "${CSE_SDK_DIR}/dedicated/sys_ded.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/main.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/DedicatedSession.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/DedicatedModuleSet.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/NativeGui.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/Signals.cpp")
  set_property(SOURCE "${CSE_SDK_DIR}/common/TextConsoleWin32.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/dedicated/native/TextConsolePosix.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/launcher/native/MacRuntimeSession.cpp")
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/launcher/native/VGui1HostImageOwner.cpp")
endif()
if(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/launcher/native/SocketImageOwner.cpp")
endif()
if(NOT WIN32)
  target_sources(hlds PRIVATE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp")
  if(MSVC)
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHs-c->")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
  else()
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O0>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wall>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wextra>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Werror=shorten-64-to-32>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Werror=pointer-to-int-cast>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-ffp-model=precise>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-ffp-contract=off>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=unused-parameter>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=unknown-pragmas>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=writable-strings>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=overloaded-virtual>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=deprecated-copy-with-user-provided-copy>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=deprecated-declarations>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=missing-field-initializers>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=sign-compare>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=switch>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=invalid-offsetof>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=missing-braces>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=inconsistent-missing-override>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=nontrivial-memcall>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=null-arithmetic>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=null-conversion>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=logical-op-parentheses>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=pointer-bool-conversion>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=tautological-pointer-compare>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=unused-value>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=unused-but-set-variable>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=unused-private-field>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=delete-non-abstract-non-virtual-dtor>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-error=delete-abstract-non-virtual-dtor>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Wno-overriding-option>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-Werror>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-fno-exceptions>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-frtti>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:-funwind-tables>")
  endif()
  if(MSVC)
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHs-c->")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
  else()
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-O2>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wall>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wextra>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Werror=shorten-64-to-32>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Werror=pointer-to-int-cast>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-ffp-model=precise>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-ffp-contract=off>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=unused-parameter>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=unknown-pragmas>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=writable-strings>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=overloaded-virtual>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=deprecated-copy-with-user-provided-copy>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=deprecated-declarations>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=missing-field-initializers>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=sign-compare>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=switch>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=invalid-offsetof>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=missing-braces>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=inconsistent-missing-override>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=nontrivial-memcall>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=null-arithmetic>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=null-conversion>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=logical-op-parentheses>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=pointer-bool-conversion>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=tautological-pointer-compare>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=unused-value>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=unused-but-set-variable>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=unused-private-field>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=delete-non-abstract-non-virtual-dtor>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-error=delete-abstract-non-virtual-dtor>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Wno-overriding-option>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-Werror>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-fno-exceptions>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-frtti>")
    set_property(SOURCE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-funwind-tables>")
  endif()
endif()
