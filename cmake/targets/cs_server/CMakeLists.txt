# Generated from projects/vs2026/cstrikedll.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(cs_server SHARED "mp")
target_sources(cs_server PRIVATE
  "${CSE_SDK_DIR}/cstrike/dlls/saved_callbacks.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/airtank.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/ammo.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/animating.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/animation.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bmodels.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/buttons.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/cbase.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/client.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/combat.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/doors.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/effects.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/explode.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/func_break.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/func_tank.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/game.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/gamerules.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/ggrenade.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/globals.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/h_ai.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/h_battery.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/h_cycler.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/h_export.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/healthkit.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hintmessage.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/items.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/lights.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/maprules.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/mortar.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/mpstubb.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/multiplay_gamerules.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/observer.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/pathcorner.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/plane.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/plats.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/player.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/singleplay_gamerules.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/skill.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/sound.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/soundent.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/spectator.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/subs.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/training_gamerules.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/triggers.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/util.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/vehicle.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/weapons.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/world.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/tutor.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/career_tasks.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/tutor_base_tutor.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/tutor_cs_tutor.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/tutor_base_states.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/tutor_cs_states.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/weapontype.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/debug.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_ak47.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_aug.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_awp.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_c4.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_deagle.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_elite.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_fiveseven.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_flashbang.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_g3sg1.cpp"
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
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_famas.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_galil.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/wpn_shared/wpn_xm1014.cpp"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_shared.c"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_math.c"
  "${CSE_SDK_DIR}/cstrike/pm_shared/pm_debug.c"
  "${CSE_SDK_DIR}/cstrike/game_shared/voice_gamemgr.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/shared_util.cpp"
  "${CSE_SDK_DIR}/cstrike/public/MemPool.cpp"
  "${CSE_SDK_DIR}/cstrike/public/utlsymbol.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/states/hostage_animate.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/states/hostage_escape.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/states/hostage_follow.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/states/hostage_idle.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/states/hostage_retreat.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/hostage.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/hostage_improv.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/hostage/hostage_localnav.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_manager.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_profile.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/bot_util.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/nav_area.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/nav_file.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/nav_node.cpp"
  "${CSE_SDK_DIR}/cstrike/game_shared/bot/nav_path.cpp"
  "${CSE_SDK_DIR}/cstrike/engine/unicode_strtools.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_chatter.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_event.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_init.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_learn.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_listen.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_manager.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_nav.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_pathfind.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_radio.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_statemachine.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_update.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_vision.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_weapon.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_gamestate.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_attack.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_buy.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_defuse_bomb.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_escape_from_bomb.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_fetch_bomb.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_follow.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_hide.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_hunt.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_idle.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_investigate_noise.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_move_to.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_plant_bomb.cpp"
  "${CSE_SDK_DIR}/cstrike/dlls/bot/cs_bot_use_entity.cpp"
)
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
set_property(TARGET cs_server PROPERTY CXX_STANDARD 17)
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:VALVE_DLL>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:CLIENT_WEAPONS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Debug>:UTL_ENGINE_ASSERTS>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/engine>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls/hostage>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll/hl>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/cl_dll/VGUI>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/pm_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/vgui_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/vgui>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/public/game_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/vgui2/controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/vgui2/game_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/dlls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/external>")
set_property(TARGET cs_server APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Debug>:/OPT:NOICF>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Debug>:/WX>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
cse_game_module_exports(cs_server "${CSE_SDK_DIR}/projects/vs2026/cstrikedll.def" "${CSE_ROOT}/cmake/exports/cs_server.def")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/std:c++17>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W4>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/WX>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/EHsc>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Oi>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GL>")
target_compile_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP4>")
set_property(TARGET cs_server PROPERTY CXX_STANDARD 17)
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:VALVE_DLL>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:CLIENT_WEAPONS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:_ITERATOR_DEBUG_LEVEL=0>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_compile_definitions(cs_server PRIVATE "$<$<CONFIG:Release>:UTL_ENGINE_ASSERTS>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/engine>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/dlls/hostage>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll/hl>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/cl_dll/VGUI>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/common>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/pm_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/game_shared/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/vgui_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/vgui>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/public/game_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/vgui2/controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/cstrike/vgui2/game_controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/dlls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared/bot>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/controls>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/vgui2/src/public>")
target_include_directories(cs_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/external>")
set_property(TARGET cs_server APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/OPT:NOICF>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(cs_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/WX>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/LTCG>")
target_link_options(cs_server PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
set_property(SOURCE "${CSE_SDK_DIR}/cstrike/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/std:c++17>")
cse_runtime_directory(cs_server "cstrike/dlls")
