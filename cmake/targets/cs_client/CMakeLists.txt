# Generated from projects/vs2026/cstrike_cdll.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(cs_client SHARED "client")
target_sources(cs_client PRIVATE
  "${CSE_SDK_DIR}/cstrike/cl_dll/ev_common.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hl/hl_baseentity.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hl/hl_events.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hl/hl_objects.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hl/hl_weapons.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/studio_util.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/ev_hldm.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/backgroundpanel.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/basecareermenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buymenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buysubmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/CareerBox.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/careermatchendmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/careerroundendmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/counterstrikeviewport.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/counterstrikeviewport_interface.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/creditsmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/CSProgressBar.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/cstrikeclassmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/cstrikespectatorgui.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/cstriketeammenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/inputw32.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/message.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buypreset_editmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buypreset_listbox.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buypreset_mainmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buypresetimageinfo.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/VGUI/buypresetpanel.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/ammo.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/ammo_secondary.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/ammohistory.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/battery.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/cdll_int.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/com_weapons.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/death.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/demo.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/entity.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/events.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/flashlight.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/GameStudioModelRenderer.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/geiger.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/health.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_iga.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_msg.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_redraw.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_spectator.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_update.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/hud_vgui2print.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/in_camera.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/input.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/interpolation.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/menu.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/nightvision.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/saytext.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/ScenarioStatus.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/SniperScope.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/status_icons.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/statusbar.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/StudioModelRenderer.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/text_message.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/train.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/tri.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/util.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/view.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/z_weather.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/buy_preset_debug.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/buy_presets.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/buy_preset_weapon_info.cpp"
  "${CSE_SDK_DIR}/cstrike/cl_dll/buy_preset.cpp"
  "${CSE_SDK_DIR}/cstrike/public/interface.cpp"
  "${CSE_SDK_DIR}/cstrike/public/checksum_crc.cpp"
  "${CSE_SDK_DIR}/cstrike/common/steamid.cpp"
  "${CSE_SDK_DIR}/cstrike/common/parsemsg.cpp"
  "${CSE_SDK_DIR}/cstrike/common/Random.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/voice_status_hud.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/shared_util.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/voice_banmgr.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/voice_status.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_ak47.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_aug.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_awp.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_c4.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_deagle.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_elite.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_famas.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_fiveseven.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_flashbang.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_g3sg1.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_galil.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_glock18.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_hegrenade.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_knife.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_m249.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_m3.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_m4a1.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_mac10.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_mp5navy.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_p228.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_p90.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_scout.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_sg550.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_sg552.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_smokegrenade.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_tmp.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_ump45.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_usp.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_xm1014.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/weapontype.cpp"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_debug.c"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_math.c"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_shared.c"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/BitmapImagePanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/BuildGroup.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Button.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ComboBox.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/controls.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/EditablePanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/FocusNavGroup.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Frame.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ImageList.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ImagePanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Label.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Menu.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/MenuButton.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/MenuItem.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/MessageBox.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Panel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ProgressBar.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/RadioButton.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/RichText.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ScrollBar.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ScrollBarSlider.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/SectionedListPanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/TextEntry.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/TextImage.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/ToggleButton.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Tooltip.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/URLLabel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/WizardPanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/WizardSubPanel.cpp"
  "${CSE_SDK_DIR}/cstrike/public/KeyValues.cpp"
  "${CSE_SDK_DIR}/cstrike/public/UtlBuffer.cpp"
  "${CSE_SDK_DIR}/cstrike/public/utlsymbol.cpp"
  "${CSE_SDK_DIR}/cstrike/public/MemPool.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/AnimatingImagePanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/BuildModeDialog.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/CheckButton.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Divider.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/HTML.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/Image.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/PanelListPanel.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/controls/QueryBox.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/classmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/ClientScoreBoardDialog.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/CreateCommandMenu.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/SpectatorGUI.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/teammenu.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/vgui_avatarimage.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/vgui_TeamFortressViewport.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/vgui_TeamFortressViewport_msgs.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/vguitextwindow.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/clientmotd.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/commandmenu.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/TutorWindowManager.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/TutorStateWindow.cpp"
  "${CSE_SDK_DIR}/cstrike/vgui2/game_controls/TutorTextWindow.cpp"
)
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
set_property(TARGET cs_client PROPERTY CXX_STANDARD 17)
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:CLIENT_DLL>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:HL_DLL>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:CLIENT_WEAPONS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Debug>:UTL_ENGINE_ASSERTS>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL3/include>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/steamworks>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/engine>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls/hostage>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll/hl>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll/VGUI>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/pm_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/vgui_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/vgui>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/game_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/vgui2/controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/vgui2/game_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/dlls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(TARGET cs_client APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_sources(cs_client PRIVATE "${CSE_SDK_DIR}/projects/vs2026/cstrike_cdll.def")
cse_link_sdl3(cs_client Debug)
cse_link_import(cs_client Debug "${CSE_SDK_DIR}/lib/public/steam_api.lib")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Oi>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
set_property(TARGET cs_client PROPERTY CXX_STANDARD 17)
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:CLIENT_DLL>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:HL_DLL>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:CLIENT_WEAPONS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_compile_definitions(cs_client PRIVATE "$<$<CONFIG:Release>:UTL_ENGINE_ASSERTS>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL3/include>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/steamworks>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/engine>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls/hostage>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll/hl>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll/VGUI>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/pm_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/vgui_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/vgui>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/game_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/vgui2/controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/vgui2/game_controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/dlls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared/bot>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(cs_client PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(TARGET cs_client APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(cs_client PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(cs_client PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
cse_link_sdl3(cs_client Release)
cse_link_import(cs_client Release "${CSE_SDK_DIR}/lib/public/steam_api.lib")
cse_runtime_directory(cs_client "cstrike/cl_dlls")
