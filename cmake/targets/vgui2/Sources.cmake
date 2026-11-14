# Authoritative CMake build definition; edit this file directly.
target_sources(vgui2 PRIVATE
  "${CSE_SDK_DIR}/common/image_codec.cpp"
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
if(APPLE)
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/native/MacFontDiscovery.cpp")
  set_property(SOURCE "${CSE_SDK_DIR}/vgui2/src/Surface.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/SurfaceSDL.cpp")
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/native/StandaloneWindow.cpp")
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/native/StandaloneRaster.cpp")
  set_property(SOURCE "${CSE_SDK_DIR}/vgui2/src/System.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/SystemMac.cpp")
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/NativeSystemClock.cpp")
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/vgui2/src/NativeClipboard.cpp")
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/common/mac_settings_store.cpp")
  cse_link_native_font_services(vgui2)
endif()
if(NOT WIN32)
  target_sources(vgui2 PRIVATE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp")
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
