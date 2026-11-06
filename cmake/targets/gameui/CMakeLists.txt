# Generated from projects/vs2026/GameUI.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(gameui SHARED "GameUI")
target_sources(gameui PRIVATE
  "${CSE_SDK_DIR}/GameUI/BackgroundMenuButton.cpp"
  "${CSE_SDK_DIR}/GameUI/BasePanel.cpp"
  "${CSE_SDK_DIR}/GameUI/BitmapButton.cpp"
  "${CSE_SDK_DIR}/GameUI/BitmapImagePanel.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerBaseFrame.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerBotFrame.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerBox.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerDefinitions.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerGame.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerGameTasks.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerMapFrame.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerProfile.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerProfileButton.cpp"
  "${CSE_SDK_DIR}/GameUI/CareerProfileFrame.cpp"
  "${CSE_SDK_DIR}/GameUI/ChangeGameDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/CDKeyEntryDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/CommandCheckButton.cpp"
  "${CSE_SDK_DIR}/GameUI/ContentControlDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/CreateMultiplayerGameBotPage.cpp"
  "${CSE_SDK_DIR}/GameUI/CreateMultiplayerGameDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/CreateMultiplayerGameGameplayPage.cpp"
  "${CSE_SDK_DIR}/GameUI/CreateMultiplayerGameServerPage.cpp"
  "${CSE_SDK_DIR}/GameUI/CvarLatch.cpp"
  "${CSE_SDK_DIR}/GameUI/CvarNegateCheckButton.cpp"
  "${CSE_SDK_DIR}/GameUI/CvarSlider.cpp"
  "${CSE_SDK_DIR}/GameUI/CvarTextEntry.cpp"
  "${CSE_SDK_DIR}/GameUI/CvarToggleCheckButton.cpp"
  "${CSE_SDK_DIR}/GameUI/DemoEditDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/DemoEventsDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/DemoPlayerDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/DemoPlayerFileDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/DottedLabel.cpp"
  "${CSE_SDK_DIR}/GameUI/GameConsole.cpp"
  "${CSE_SDK_DIR}/GameUI/GameConsoleDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/GameUI_Interface.cpp"
  "${CSE_SDK_DIR}/GameUI/HelpDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/ImageButton.cpp"
  "${CSE_SDK_DIR}/GameUI/KeyToggleCheckButton.cpp"
  "${CSE_SDK_DIR}/GameUI/LabeledCommandComboBox.cpp"
  "${CSE_SDK_DIR}/GameUI/labelslidercombo.cpp"
  "${CSE_SDK_DIR}/GameUI/LoadGameDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/LoadingDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/LogoFile.cpp"
  "${CSE_SDK_DIR}/GameUI/ModInfo.cpp"
  "${CSE_SDK_DIR}/GameUI/MultiplayerAdvancedDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/MusicManager.cpp"
  "${CSE_SDK_DIR}/GameUI/NewGameDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubAdvanced.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubAudio.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubKeyboard.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubMouse.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubMultiplayer.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubVideo.cpp"
  "${CSE_SDK_DIR}/GameUI/OptionsSubVoice.cpp"
  "${CSE_SDK_DIR}/GameUI/PanelListPanel.cpp"
  "${CSE_SDK_DIR}/GameUI/PlayerListDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/RefreshSteamLogin.cpp"
  "${CSE_SDK_DIR}/GameUI/RunGameEngine.cpp"
  "${CSE_SDK_DIR}/GameUI/SaveGameDialog.cpp"
  "${CSE_SDK_DIR}/GameUI/ScriptObject.cpp"
  "${CSE_SDK_DIR}/GameUI/Sys_Utils.cpp"
  "${CSE_SDK_DIR}/GameUI/Taskbar.cpp"
  "${CSE_SDK_DIR}/GameUI/TaskButton.cpp"
  "${CSE_SDK_DIR}/GameUI/TextEntryBox.cpp"
  "${CSE_SDK_DIR}/GameUI/vcontrolslistpanel.cpp"
  "${CSE_SDK_DIR}/GameUI/VGuiSystemModuleLoader.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/AnimatingImagePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BitmapImagePanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BuildGroup.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/BuildModeDialog.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Button.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/CheckButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ComboBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/controls.cpp"
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
  "${CSE_SDK_DIR}/vgui2/controls/PanelHorizListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PanelListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ProgressBar.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PropertyDialog.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PropertyPage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/PropertySheet.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/QueryBox.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/RadioButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/RichText.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBar.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ScrollBarSlider.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/SectionedListPanel.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Slider.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextEntry.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/TextImage.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/ToggleButton.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/Tooltip.cpp"
  "${CSE_SDK_DIR}/vgui2/controls/URLLabel.cpp"
  "${CSE_SDK_DIR}/engine/interface.cpp"
  "${CSE_SDK_DIR}/public/checksum_md5.cpp"
  "${CSE_SDK_DIR}/engine/utlbuffer.cpp"
  "${CSE_SDK_DIR}/engine/utlsymbol.cpp"
  "${CSE_SDK_DIR}/engine/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/cstrike/common/parsemsg.cpp"
  "${CSE_SDK_DIR}/cstrike/common/Random.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/shared_util.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/weapontype.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp"
  "${CSE_SDK_DIR}/HLTV/common/byteorder.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp"
  "${CSE_SDK_DIR}/public/characterset.cpp"
  "${CSE_SDK_DIR}/vgui2/src/public/filesystem_helpers.cpp"
  "${CSE_SDK_DIR}/engine/KeyValues.cpp"
  "${CSE_SDK_DIR}/engine/MemPool.cpp"
  "${CSE_SDK_DIR}/engine/controls.cpp"
)
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/external:I${CSE_SDK_DIR}/external/SDL2>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze:external->")
set_property(TARGET gameui PROPERTY CXX_STANDARD 17)
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:GAMEUI_DLL>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:CLIENT_DLL>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(TARGET gameui APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/OPT:REF>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/LARGEADDRESSAWARE>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/MACHINE:X86>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
target_sources(gameui PRIVATE "${CSE_SDK_DIR}/projects/vs2026/GameUI.def")
cse_link_import(gameui Debug "${CSE_SDK_DIR}/lib/public/SDL2.lib")
cse_link_import(gameui Debug "${CSE_SDK_DIR}/lib/public/steam_api.lib")
target_link_libraries(gameui PRIVATE "$<$<CONFIG:Debug>:user32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/GameUI/LogoFile.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/Ob1>")
set_property(SOURCE "${CSE_SDK_DIR}/GameUI/OptionsSubKeyboard.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/Ob1>")
set_property(SOURCE "${CSE_SDK_DIR}/GameUI/OptionsSubMultiplayer.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/Ob1>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Debug>:${CSE_SDK_DIR}>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/fp:precise>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/external:I${CSE_SDK_DIR}/external/SDL2>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze>")
target_compile_options(gameui PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/analyze:external->")
set_property(TARGET gameui PROPERTY CXX_STANDARD 17)
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:GAMEUI_DLL>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:CLIENT_DLL>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:UTL_ENGINE_ASSERTS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(gameui PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
target_include_directories(gameui PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(TARGET gameui APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/LARGEADDRESSAWARE>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/RELEASE>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/MACHINE:X86>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(gameui PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
cse_link_import(gameui Release "${CSE_SDK_DIR}/lib/public/SDL2.lib")
cse_link_import(gameui Release "${CSE_SDK_DIR}/lib/public/steam_api.lib")
target_link_libraries(gameui PRIVATE "$<$<CONFIG:Release>:user32.lib>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/common/ObjectList.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/HLTV/DemoPlayer/private>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/GameUI>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cl_dll>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external/SDL2>")
set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "$<$<CONFIG:Release>:${CSE_SDK_DIR}>")
target_link_libraries(gameui PRIVATE tier0)
cse_runtime_directory(gameui "valve/cl_dlls")
