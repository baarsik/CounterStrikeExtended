# Generated from projects/vs2026/hldll.vcxproj; see tools/translate_msbuild.py.
cse_add_runtime(hl_server SHARED "hl")
target_sources(hl_server PRIVATE
  "${CSE_SDK_DIR}/dlls/saved_callbacks.cpp"
  "${CSE_SDK_DIR}/dlls/aflock.cpp"
  "${CSE_SDK_DIR}/dlls/agrunt.cpp"
  "${CSE_SDK_DIR}/dlls/airtank.cpp"
  "${CSE_SDK_DIR}/dlls/animating.cpp"
  "${CSE_SDK_DIR}/dlls/animation.cpp"
  "${CSE_SDK_DIR}/dlls/apache.cpp"
  "${CSE_SDK_DIR}/dlls/barnacle.cpp"
  "${CSE_SDK_DIR}/dlls/barney.cpp"
  "${CSE_SDK_DIR}/dlls/bigmomma.cpp"
  "${CSE_SDK_DIR}/dlls/bloater.cpp"
  "${CSE_SDK_DIR}/dlls/bmodels.cpp"
  "${CSE_SDK_DIR}/dlls/bullsquid.cpp"
  "${CSE_SDK_DIR}/dlls/buttons.cpp"
  "${CSE_SDK_DIR}/dlls/cbase.cpp"
  "${CSE_SDK_DIR}/dlls/client.cpp"
  "${CSE_SDK_DIR}/dlls/combat.cpp"
  "${CSE_SDK_DIR}/dlls/controller.cpp"
  "${CSE_SDK_DIR}/dlls/crossbow.cpp"
  "${CSE_SDK_DIR}/dlls/crowbar.cpp"
  "${CSE_SDK_DIR}/dlls/defaultai.cpp"
  "${CSE_SDK_DIR}/dlls/doors.cpp"
  "${CSE_SDK_DIR}/dlls/effects.cpp"
  "${CSE_SDK_DIR}/dlls/egon.cpp"
  "${CSE_SDK_DIR}/dlls/explode.cpp"
  "${CSE_SDK_DIR}/dlls/flyingmonster.cpp"
  "${CSE_SDK_DIR}/dlls/func_break.cpp"
  "${CSE_SDK_DIR}/dlls/func_tank.cpp"
  "${CSE_SDK_DIR}/dlls/game.cpp"
  "${CSE_SDK_DIR}/dlls/gamerules.cpp"
  "${CSE_SDK_DIR}/dlls/gargantua.cpp"
  "${CSE_SDK_DIR}/dlls/gauss.cpp"
  "${CSE_SDK_DIR}/dlls/genericmonster.cpp"
  "${CSE_SDK_DIR}/dlls/ggrenade.cpp"
  "${CSE_SDK_DIR}/dlls/globals.cpp"
  "${CSE_SDK_DIR}/dlls/gman.cpp"
  "${CSE_SDK_DIR}/dlls/handgrenade.cpp"
  "${CSE_SDK_DIR}/dlls/hassassin.cpp"
  "${CSE_SDK_DIR}/dlls/headcrab.cpp"
  "${CSE_SDK_DIR}/dlls/healthkit.cpp"
  "${CSE_SDK_DIR}/dlls/hgrunt.cpp"
  "${CSE_SDK_DIR}/dlls/hornet.cpp"
  "${CSE_SDK_DIR}/dlls/hornetgun.cpp"
  "${CSE_SDK_DIR}/dlls/houndeye.cpp"
  "${CSE_SDK_DIR}/dlls/h_ai.cpp"
  "${CSE_SDK_DIR}/dlls/h_battery.cpp"
  "${CSE_SDK_DIR}/dlls/h_cine.cpp"
  "${CSE_SDK_DIR}/dlls/h_cycler.cpp"
  "${CSE_SDK_DIR}/dlls/h_export.cpp"
  "${CSE_SDK_DIR}/dlls/ichthyosaur.cpp"
  "${CSE_SDK_DIR}/dlls/islave.cpp"
  "${CSE_SDK_DIR}/dlls/items.cpp"
  "${CSE_SDK_DIR}/dlls/leech.cpp"
  "${CSE_SDK_DIR}/dlls/lights.cpp"
  "${CSE_SDK_DIR}/dlls/maprules.cpp"
  "${CSE_SDK_DIR}/dlls/monstermaker.cpp"
  "${CSE_SDK_DIR}/dlls/monsters.cpp"
  "${CSE_SDK_DIR}/dlls/monsterstate.cpp"
  "${CSE_SDK_DIR}/dlls/mortar.cpp"
  "${CSE_SDK_DIR}/dlls/mp5.cpp"
  "${CSE_SDK_DIR}/dlls/multiplay_gamerules.cpp"
  "${CSE_SDK_DIR}/dlls/nihilanth.cpp"
  "${CSE_SDK_DIR}/dlls/nodes.cpp"
  "${CSE_SDK_DIR}/dlls/observer.cpp"
  "${CSE_SDK_DIR}/dlls/osprey.cpp"
  "${CSE_SDK_DIR}/dlls/pathcorner.cpp"
  "${CSE_SDK_DIR}/dlls/plane.cpp"
  "${CSE_SDK_DIR}/dlls/plats.cpp"
  "${CSE_SDK_DIR}/dlls/player.cpp"
  "${CSE_SDK_DIR}/dlls/python.cpp"
  "${CSE_SDK_DIR}/dlls/rat.cpp"
  "${CSE_SDK_DIR}/dlls/roach.cpp"
  "${CSE_SDK_DIR}/dlls/rpg.cpp"
  "${CSE_SDK_DIR}/dlls/satchel.cpp"
  "${CSE_SDK_DIR}/dlls/schedule.cpp"
  "${CSE_SDK_DIR}/dlls/scientist.cpp"
  "${CSE_SDK_DIR}/dlls/scripted.cpp"
  "${CSE_SDK_DIR}/dlls/shotgun.cpp"
  "${CSE_SDK_DIR}/dlls/singleplay_gamerules.cpp"
  "${CSE_SDK_DIR}/dlls/skill.cpp"
  "${CSE_SDK_DIR}/dlls/sound.cpp"
  "${CSE_SDK_DIR}/dlls/soundent.cpp"
  "${CSE_SDK_DIR}/dlls/spectator.cpp"
  "${CSE_SDK_DIR}/dlls/squadmonster.cpp"
  "${CSE_SDK_DIR}/dlls/squeakgrenade.cpp"
  "${CSE_SDK_DIR}/dlls/subs.cpp"
  "${CSE_SDK_DIR}/dlls/talkmonster.cpp"
  "${CSE_SDK_DIR}/dlls/teamplay_gamerules.cpp"
  "${CSE_SDK_DIR}/dlls/tempmonster.cpp"
  "${CSE_SDK_DIR}/dlls/tentacle.cpp"
  "${CSE_SDK_DIR}/dlls/triggers.cpp"
  "${CSE_SDK_DIR}/dlls/tripmine.cpp"
  "${CSE_SDK_DIR}/dlls/turret.cpp"
  "${CSE_SDK_DIR}/dlls/util.cpp"
  "${CSE_SDK_DIR}/dlls/weapons.cpp"
  "${CSE_SDK_DIR}/dlls/world.cpp"
  "${CSE_SDK_DIR}/dlls/wpn_shared/hl_wpn_glock.cpp"
  "${CSE_SDK_DIR}/dlls/xen.cpp"
  "${CSE_SDK_DIR}/dlls/zombie.cpp"
  "${CSE_SDK_DIR}/game_shared/voice_gamemgr.cpp"
  "${CSE_SDK_DIR}/pm_shared/pm_debug.c"
  "${CSE_SDK_DIR}/pm_shared/pm_math.c"
  "${CSE_SDK_DIR}/pm_shared/pm_shared.c"
)
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/W3>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/Od>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:WIN32>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:_WINDOWS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:QUIVER>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:VOXEL>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:QUAKE2>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:VALVE_DLL>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:CLIENT_WEAPONS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Debug>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/dlls>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/common>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/public>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Debug>:${CSE_SDK_DIR}/engine>")
set_property(TARGET hl_server APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebug>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Debug>:/DEBUG>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Debug>:/SUBSYSTEM:WINDOWS>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL>")
cse_game_module_exports(hl_server "${CSE_SDK_DIR}/dlls/hl.def" "${CSE_ROOT}/cmake/exports/hl_server.def")
set_property(SOURCE "${CSE_SDK_DIR}/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/std:c++17>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W3>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GR>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Gy>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Oi>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GL>")
target_compile_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/MP>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:WIN32>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:NDEBUG>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:_WINDOWS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:QUIVER>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:VOXEL>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:QUAKE2>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:VALVE_DLL>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:CLIENT_WEAPONS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:_CRT_SECURE_NO_WARNINGS>")
target_compile_definitions(hl_server PRIVATE "$<$<CONFIG:Release>:_CRT_NONSTDC_NO_WARNINGS>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/dlls>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/common>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/pm_shared>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/game_shared>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/public>")
target_include_directories(hl_server PRIVATE "$<$<CONFIG:Release>:${CSE_SDK_DIR}/engine>")
set_property(TARGET hl_server APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreaded>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Release>:/OPT:NOICF>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(hl_server PRIVATE "$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:/DEBUG>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Release>:/LTCG>")
target_link_options(hl_server PRIVATE "$<$<CONFIG:Release>:/INCREMENTAL:NO>")
set_property(SOURCE "${CSE_SDK_DIR}/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/EHsc>")
set_property(SOURCE "${CSE_SDK_DIR}/dlls/saved_callbacks.cpp" APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:/std:c++17>")
cse_runtime_directory(hl_server "valve/dlls")
