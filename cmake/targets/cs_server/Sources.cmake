# Authoritative CMake build definition; edit this file directly.
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
if(NOT WIN32)
  target_sources(cs_server PRIVATE "${CSE_SDK_DIR}/common/legacy_wide_format.cpp")
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
