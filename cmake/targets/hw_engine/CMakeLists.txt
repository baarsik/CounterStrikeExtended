# Generated from projects/vs2026/hw_engine.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(hw_engine SHARED "hw")
target_sources(hw_engine PRIVATE
  "${CSE_SDK_DIR}/engine/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/engine/bitmap_win.cpp"
  "${CSE_SDK_DIR}/engine/buildnum.cpp"
  "${CSE_SDK_DIR}/engine/cd.cpp"
  "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp"
  "${CSE_SDK_DIR}/engine/mpg123_music.cpp"
  "${CSE_SDK_DIR}/engine/cdll_exp.cpp"
  "${CSE_SDK_DIR}/engine/cdll_int.cpp"
  "${CSE_SDK_DIR}/engine/chase.cpp"
  "${CSE_SDK_DIR}/engine/CircularBuffer.cpp"
  "${CSE_SDK_DIR}/engine/cl_demo.cpp"
  "${CSE_SDK_DIR}/engine/cl_draw.cpp"
  "${CSE_SDK_DIR}/engine/cl_ents.cpp"
  "${CSE_SDK_DIR}/engine/cl_extrap.cpp"
  "${CSE_SDK_DIR}/engine/cl_main.cpp"
  "${CSE_SDK_DIR}/engine/cl_parse.cpp"
  "${CSE_SDK_DIR}/engine/cl_parsefn.cpp"
  "${CSE_SDK_DIR}/engine/cl_pmove.cpp"
  "${CSE_SDK_DIR}/engine/cl_pred.cpp"
  "${CSE_SDK_DIR}/engine/cl_spectator.cpp"
  "${CSE_SDK_DIR}/engine/cl_tent.cpp"
  "${CSE_SDK_DIR}/engine/cmd.cpp"
  "${CSE_SDK_DIR}/engine/cmodel.cpp"
  "${CSE_SDK_DIR}/engine/com_custom.cpp"
  "${CSE_SDK_DIR}/engine/console.cpp"
  "${CSE_SDK_DIR}/engine/crc.cpp"
  "${CSE_SDK_DIR}/engine/cvar.cpp"
  "${CSE_SDK_DIR}/engine/czmodelcheck.cpp"
  "${CSE_SDK_DIR}/engine/decals.cpp"
  "${CSE_SDK_DIR}/engine/delta.cpp"
  "${CSE_SDK_DIR}/engine/DemoPlayerWrapper.cpp"
  "${CSE_SDK_DIR}/engine/DetailTexture.cpp"
  "${CSE_SDK_DIR}/engine/download.cpp"
  "${CSE_SDK_DIR}/engine/eventapi.cpp"
  "${CSE_SDK_DIR}/engine/filesystem.cpp"
  "${CSE_SDK_DIR}/engine/filesystem_internal.cpp"
  "${CSE_SDK_DIR}/engine/glHud.cpp"
  "${CSE_SDK_DIR}/engine/gl_draw.cpp"
  "${CSE_SDK_DIR}/engine/gl_mesh.cpp"
  "${CSE_SDK_DIR}/engine/gl_model.cpp"
  "${CSE_SDK_DIR}/engine/gl_refrag.cpp"
  "${CSE_SDK_DIR}/engine/gl_rlight.cpp"
  "${CSE_SDK_DIR}/engine/gl_rmain.cpp"
  "${CSE_SDK_DIR}/engine/gl_rmisc.cpp"
  "${CSE_SDK_DIR}/engine/gl_rsurf.cpp"
  "${CSE_SDK_DIR}/engine/gl_screen.cpp"
  "${CSE_SDK_DIR}/engine/gl_vidnt.cpp"
  "${CSE_SDK_DIR}/engine/gl_warp.cpp"
  "${CSE_SDK_DIR}/engine/hashpak.cpp"
  "${CSE_SDK_DIR}/engine/host.cpp"
  "${CSE_SDK_DIR}/engine/host_cmd.cpp"
  "${CSE_SDK_DIR}/engine/hud.cpp"
  "${CSE_SDK_DIR}/engine/info.cpp"
  "${CSE_SDK_DIR}/engine/interface.cpp"
  "${CSE_SDK_DIR}/engine/ipratelimit.cpp"
  "${CSE_SDK_DIR}/engine/ipratelimitWrapper.cpp"
  "${CSE_SDK_DIR}/engine/keys.cpp"
  "${CSE_SDK_DIR}/engine/KeyValues.cpp"
  "${CSE_SDK_DIR}/engine/LoadBlob.cpp"
  "${CSE_SDK_DIR}/engine/l_studio.cpp"
  "${CSE_SDK_DIR}/engine/mathlib.cpp"
  "${CSE_SDK_DIR}/engine/mem.cpp"
  "${CSE_SDK_DIR}/engine/MemPool.cpp"
  "${CSE_SDK_DIR}/engine/module.cpp"
  "${CSE_SDK_DIR}/engine/net_api.cpp"
  "${CSE_SDK_DIR}/engine/net_chan.cpp"
  "${CSE_SDK_DIR}/engine/net_ws.cpp"
  "${CSE_SDK_DIR}/engine/pmove.cpp"
  "${CSE_SDK_DIR}/engine/pmovedbg.cpp"
  "${CSE_SDK_DIR}/engine/pmovetst.cpp"
  "${CSE_SDK_DIR}/engine/pr_cmds.cpp"
  "${CSE_SDK_DIR}/engine/pr_edict.cpp"
  "${CSE_SDK_DIR}/engine/qgl.cpp"
  "${CSE_SDK_DIR}/engine/registry.cpp"
  "${CSE_SDK_DIR}/engine/r_efx.cpp"
  "${CSE_SDK_DIR}/engine/r_part.cpp"
  "${CSE_SDK_DIR}/engine/r_studio.cpp"
  "${CSE_SDK_DIR}/engine/r_trans.cpp"
  "${CSE_SDK_DIR}/engine/r_triangle.cpp"
  "${CSE_SDK_DIR}/engine/Sequence.cpp"
  "${CSE_SDK_DIR}/engine/snd_dma.cpp"
  "${CSE_SDK_DIR}/engine/snd_mem.cpp"
  "${CSE_SDK_DIR}/engine/snd_mix.cpp"
  "${CSE_SDK_DIR}/engine/snd_sdl.cpp"
  "${CSE_SDK_DIR}/engine/snd_win.cpp"
  "${CSE_SDK_DIR}/engine/steamid.cpp"
  "${CSE_SDK_DIR}/engine/sv_log.cpp"
  "${CSE_SDK_DIR}/engine/sv_main.cpp"
  "${CSE_SDK_DIR}/engine/sv_move.cpp"
  "${CSE_SDK_DIR}/engine/sv_phys.cpp"
  "${CSE_SDK_DIR}/engine/sv_pmove.cpp"
  "${CSE_SDK_DIR}/engine/sv_remoteaccess.cpp"
  "${CSE_SDK_DIR}/engine/sv_secure.cpp"
  "${CSE_SDK_DIR}/engine/sv_steam3.cpp"
  "${CSE_SDK_DIR}/engine/sv_upld.cpp"
  "${CSE_SDK_DIR}/engine/sv_user.cpp"
  "${CSE_SDK_DIR}/engine/SystemWrapper.cpp"
  "${CSE_SDK_DIR}/engine/sys_dll.cpp"
  "${CSE_SDK_DIR}/engine/sys_dll2.cpp"
  "${CSE_SDK_DIR}/engine/sys_engine.cpp"
  "${CSE_SDK_DIR}/engine/sys_getmodes.cpp"
  "${CSE_SDK_DIR}/engine/sys_sdlwind.cpp"
  "${CSE_SDK_DIR}/engine/textures.cpp"
  "${CSE_SDK_DIR}/engine/thread.cpp"
  "${CSE_SDK_DIR}/engine/tmessage.cpp"
  "${CSE_SDK_DIR}/engine/traceinit.cpp"
  "${CSE_SDK_DIR}/engine/unicode_strtools.cpp"
  "${CSE_SDK_DIR}/engine/utlbuffer.cpp"
  "${CSE_SDK_DIR}/engine/utlsymbol.cpp"
  "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface.cpp"
  "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface_Font.cpp"
  "${CSE_SDK_DIR}/engine/vgui2/BaseUI_Interface.cpp"
  "${CSE_SDK_DIR}/engine/vgui2/FontTextureCache.cpp"
  "${CSE_SDK_DIR}/engine/vgui2/text_draw.cpp"
  "${CSE_SDK_DIR}/engine/vgui_EngineSurface.cpp"
  "${CSE_SDK_DIR}/engine/vgui_EngineSurfaceHW.cpp"
  "${CSE_SDK_DIR}/engine/vgui_int.cpp"
  "${CSE_SDK_DIR}/engine/vgui_intwrap.cpp"
  "${CSE_SDK_DIR}/engine/vgui_intwrap2.cpp"
  "${CSE_SDK_DIR}/engine/view.cpp"
  "${CSE_SDK_DIR}/engine/voice.cpp"
  "${CSE_SDK_DIR}/engine/voice_gain.cpp"
  "${CSE_SDK_DIR}/engine/voice_mixer_controls.cpp"
  "${CSE_SDK_DIR}/engine/voice_record_dsound.cpp"
  "${CSE_SDK_DIR}/engine/voice_record_wavein.cpp"
  "${CSE_SDK_DIR}/engine/voice_sound_engine_interface.cpp"
  "${CSE_SDK_DIR}/engine/voice_wavefile.cpp"
  "${CSE_SDK_DIR}/engine/wad.cpp"
  "${CSE_SDK_DIR}/engine/webm_video.cpp"
  "${CSE_SDK_DIR}/engine/world.cpp"
  "${CSE_SDK_DIR}/engine/yuv2rgb.cpp"
  "${CSE_SDK_DIR}/engine/zone.cpp"
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
  "${CSE_SDK_DIR}/vgui2/controls/Menu.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MenuButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MenuItem.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/MessageBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Panel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PanelListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ProgressBar.cpp"
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
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontAmalgam.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontManager.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/linuxfont.cpp"
  "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/vgui_key_translation.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/common/TokenLine.cpp"
  "${CSE_SDK_DIR}/engine/common.cpp"
  "${CSE_SDK_DIR}/engine/controls.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/controls.cpp"
  "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp"
  "${CSE_SDK_DIR}/engine/engine.rc"
)
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/arch:SSE2>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET hw_engine PROPERTY CXX_STANDARD 17)
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Debug>:CSE_ENGINE_WSOCK32=1>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Debug>:HL25_HW_ENGINE>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Debug>:LINK_MPG123_DLL>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include/webm>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/include>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL3/include>")
set_property(TARGET hw_engine APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Debug>:/MAP:${CMAKE_BINARY_DIR}/symbols/Debug/hw_engine.map>")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/bz2d.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/freetyped.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/libexpatdMD.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/libpng16d.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/zsd.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/lib/public/steam_api.lib")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Debug>:opengl32.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Debug>:wsock32.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Debug>:winmm.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Debug>:dxguid.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Debug>:dsound.lib>")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/brotlidec.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/brotlicommon.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/vorbisfile.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/vorbis.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/ogg.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/webm.lib")
cse_link_import(hw_engine Debug "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/debug/lib/vpx.lib")
cse_link_mpg123(hw_engine Debug)
cse_link_sdl3(hw_engine Debug)
set_property(SOURCE "${CSE_SDK_DIR}/engine/BaseSystemModule.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/bitmap_win.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/buildnum.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_music.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_music.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cdll_exp.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cdll_int.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/chase.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/CircularBuffer.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_demo.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_ents.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_extrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_main.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_parse.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_parsefn.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_pred.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_spectator.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_tent.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cmd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cmodel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/com_custom.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/console.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/crc.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cvar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/czmodelcheck.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/decals.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/delta.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/DemoPlayerWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/DetailTexture.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/download.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/eventapi.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/filesystem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/filesystem_internal.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/glHud.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_mesh.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_model.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_refrag.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rlight.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rmain.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rmisc.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rsurf.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_screen.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_vidnt.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_warp.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/hashpak.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/host.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/host_cmd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/hud.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/info.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/ipratelimit.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/ipratelimitWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/keys.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/KeyValues.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/LoadBlob.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/l_studio.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mathlib.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/MemPool.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/module.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_api.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_chan.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_ws.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmovedbg.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmovetst.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pr_cmds.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pr_edict.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/qgl.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/registry.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_efx.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_part.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_studio.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_trans.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_triangle.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/Sequence.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_dma.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_mem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_mix.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_sdl.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_win.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/steamid.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_log.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_main.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_move.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_phys.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_remoteaccess.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_secure.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_steam3.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_upld.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_user.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/SystemWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHa>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_engine.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_getmodes.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_sdlwind.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/textures.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/thread.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/tmessage.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/traceinit.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/unicode_strtools.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/utlbuffer.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/utlsymbol.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface_Font.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUI_Interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/FontTextureCache.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/text_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_EngineSurface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_EngineSurfaceHW.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_int.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_intwrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_intwrap2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/view.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_gain.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_mixer_controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_record_dsound.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_record_wavein.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_sound_engine_interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_wavefile.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/wad.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/webm_video.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/world.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/yuv2rgb.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/zone.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/AnimatingImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BitmapImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BuildGroup.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BuildModeDialog.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Button.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/CheckButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ComboBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Divider.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/EditablePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/FocusNavGroup.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Frame.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/HTML.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Image.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ImageList.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Label.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Menu.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MenuButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MenuItem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MessageBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Panel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/PanelListPanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ProgressBar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/QueryBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/RadioButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/RichText.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ScrollBar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ScrollBarSlider.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/TextEntry.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ToggleButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/URLLabel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontAmalgam.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontManager.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/linuxfont.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/vgui_key_translation.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/common/TokenLine.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/common.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:strict>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include/webm>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/utils/vgui/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/bin/obj/engdll/Win32/Debug/mpg123/source/mpg123-1.33.7/src/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL3/include>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O1>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/arch:SSE2>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/D_MBCS>")
target_compile_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
set_property(TARGET hw_engine PROPERTY CXX_STANDARD 17)
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Release>:CSE_ENGINE_WSOCK32=1>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Release>:HL25_HW_ENGINE>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hw_engine PRIVATE "$<$<CONFIG:Release>:LINK_MPG123_DLL>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include/webm>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/include>")
target_include_directories(hw_engine PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL3/include>")
set_property(TARGET hw_engine APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Release>:/OPT:NOICF>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(hw_engine PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
target_link_options(hw_engine PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/MAP:${CMAKE_BINARY_DIR}/symbols/Release/hw_engine.map>")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/bz2.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/freetype.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/libexpatMD.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/libpng16.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/zs.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/lib/public/steam_api.lib")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Release>:opengl32.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Release>:wsock32.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Release>:winmm.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Release>:dxguid.lib>")
target_link_libraries(hw_engine PRIVATE "$<$<CONFIG:Release>:dsound.lib>")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/brotlidec.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/brotlicommon.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/vorbisfile.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/vorbis.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/ogg.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/webm.lib")
cse_link_import(hw_engine Release "${CSE_SDK_DIR}/external/installed/x86-windows-static-md/lib/vpx.lib")
cse_link_mpg123(hw_engine Release)
cse_link_sdl3(hw_engine Release)
set_property(SOURCE "${CSE_SDK_DIR}/engine/BaseSystemModule.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/bitmap_win.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/buildnum.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_decoder.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_music.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mpg123_music.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cdll_exp.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cdll_int.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/chase.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/CircularBuffer.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_demo.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_ents.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_extrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_main.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_parse.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_parsefn.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_pred.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_spectator.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cl_tent.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cmd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cmodel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/com_custom.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/console.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/crc.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/cvar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/czmodelcheck.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/decals.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/delta.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/DemoPlayerWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/DetailTexture.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/download.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/eventapi.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/filesystem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/filesystem_internal.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/glHud.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_mesh.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_model.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_refrag.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rlight.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rmain.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rmisc.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_rsurf.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_screen.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_vidnt.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/gl_warp.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/hashpak.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/host.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/host_cmd.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/hud.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/info.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/ipratelimit.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/ipratelimitWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/keys.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/KeyValues.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/LoadBlob.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/l_studio.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mathlib.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/mem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/MemPool.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/module.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_api.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_chan.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/net_ws.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmovedbg.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pmovetst.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pr_cmds.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/pr_edict.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/qgl.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/registry.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_efx.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_part.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_studio.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_trans.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/r_triangle.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/Sequence.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_dma.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_mem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_mix.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_sdl.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/snd_win.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/steamid.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_log.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_main.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_move.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_phys.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_pmove.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_remoteaccess.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_secure.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_steam3.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_upld.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sv_user.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/SystemWrapper.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHa>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_dll2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_engine.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_getmodes.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/sys_sdlwind.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/textures.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/thread.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/tmessage.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/traceinit.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/unicode_strtools.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/utlbuffer.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/utlsymbol.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUISurface_Font.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/BaseUI_Interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/FontTextureCache.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui2/text_draw.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_EngineSurface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_EngineSurfaceHW.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_int.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_intwrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/vgui_intwrap2.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/view.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_gain.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_mixer_controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_record_dsound.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_record_wavein.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_sound_engine_interface.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/voice_wavefile.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/wad.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/webm_video.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/world.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/yuv2rgb.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/zone.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/AnimatingImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BitmapImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BuildGroup.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/BuildModeDialog.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Button.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/CheckButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ComboBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Divider.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/EditablePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/FocusNavGroup.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Frame.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/HTML.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Image.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ImageList.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ImagePanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Label.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Menu.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MenuButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MenuItem.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/MessageBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Panel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/PanelListPanel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ProgressBar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/QueryBox.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/RadioButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/RichText.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ScrollBar.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ScrollBarSlider.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/TextEntry.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/ToggleButton.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/URLLabel.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontAmalgam.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/FontManager.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/linuxfont.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/vgui_surfacelib/vgui_key_translation.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/common/TokenLine.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/common.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:strict>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/vgui2/controls/controls.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/fp:precise>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include/webm>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/utils/vgui/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/bin/obj/engdll/Win32/Release/mpg123/source/mpg123-1.33.7/src/include>")
set_property(SOURCE "${CSE_SDK_DIR}/engine/VGUI_EngineSurfaceWrap.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL3/include>")
target_link_libraries(hw_engine PRIVATE vgui)
cse_runtime_directory(hw_engine "")
