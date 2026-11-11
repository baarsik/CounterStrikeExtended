cmake_minimum_required(VERSION 3.25)
project(native_server_browser_ui_tests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CSE_REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CSE_BROWSER_SOURCE_ROOT "${CSE_REPO}" CACHE PATH "Actual immutable source snapshot root")
option(CSE_BROWSER_UI_SANITIZERS "Address and undefined behavior sanitizer fixtures" OFF)
option(CSE_BROWSER_UI_PRESENTATION_TESTS "Build tests against the actual new presentation component" OFF)
set(sdk "${CSE_BROWSER_SOURCE_ROOT}/GoldSrc-SDK")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/extracted")
set(extracted
    "${generated}/retirement.inc"
    "${generated}/retirement_state.inc"
    "${generated}/startup_hw.inc"
    "${generated}/startup_sw.inc"
    "${generated}/loader.inc"
    "${generated}/model_fixture.inc"
    "${generated}/owner-extraction.json"
    "${generated}/loader_guard.inc"
    "${generated}/gameui_shutdown.inc"
    "${generated}/gameui_panels.inc"
    "${generated}/panel_state.inc"
    "${generated}/baseui_shutdown.inc"
    "${generated}/baseui_surface_retirement.inc"
    "${generated}/entry_hw.inc"
    "${generated}/entry_sw.inc"
    "${generated}/listen_sw.inc"
    "${generated}/listen_hw.inc"
    "${generated}/listen_retirement.inc"
    "${generated}/listen_state.inc"
    "${generated}/host_init.inc")
add_custom_command(OUTPUT ${extracted}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/extract_owners.py"
        --repo "${CSE_BROWSER_SOURCE_ROOT}" --output "${generated}"
    DEPENDS extract_owners.py
        "${sdk}/engine/vgui_intwrap.cpp"
        "${sdk}/engine/vgui_intwrap2.cpp"
        "${sdk}/engine/vgui_int.cpp"
        "${sdk}/engine/host.cpp"
        "${sdk}/engine/sys_dll2.cpp"
        "${sdk}/engine/sys_engine.cpp"
        "${sdk}/GameUI/VGuiSystemModuleLoader.cpp"
        "${sdk}/GameUI/GameUI_Interface.cpp"
        "${sdk}/GameUI/GameConsole.cpp"
        "${sdk}/engine/vgui2/BaseUI_Interface.cpp"
        "${sdk}/engine/vgui2/BaseUISurface.cpp"
        "${CSE_BROWSER_SOURCE_ROOT}/tests/native/server_browser/model_tests.cpp"
    VERBATIM)
add_custom_target(extract_browser_ui_owners DEPENDS ${extracted})

option(CSE_BROWSER_UI_REFERENCE_LOADER_ONLY "Actual 32-bit external loader branch fixture" OFF)
if(CSE_BROWSER_UI_REFERENCE_LOADER_ONLY)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "Reference loader fixture requires an actual 32-bit compiler; no width spoofing")
    endif()
    add_executable(browser_loader_owner_tests loader_owner_tests.cpp)
    add_dependencies(browser_loader_owner_tests extract_browser_ui_owners)
    target_include_directories(browser_loader_owner_tests PRIVATE "${generated}" "${sdk}/GameUI"
        "${sdk}/engine" "${sdk}/vgui2/src/public" "${sdk}/public")
    if(MSVC)
        target_compile_options(browser_loader_owner_tests PRIVATE /W4 /WX /EHsc)
        target_compile_definitions(browser_loader_owner_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(browser_loader_owner_tests PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME browser_reference_external_selection COMMAND browser_loader_owner_tests reference-selection)
    return()
endif()

add_executable(browser_action_error_tests action_error_tests.cpp "${sdk}/GameUI/native/ServerBrowserModel.cpp")
target_include_directories(browser_action_error_tests PRIVATE "${sdk}/GameUI/native")
add_executable(browser_retirement_owner_tests retirement_owner_tests.cpp)
add_executable(browser_loader_owner_tests loader_owner_tests.cpp)
target_include_directories(browser_loader_owner_tests PRIVATE
    "${sdk}/GameUI" "${sdk}/engine" "${sdk}/vgui2/src/public" "${sdk}/public")
add_executable(browser_startup_hw_tests startup_owner_tests.cpp)
target_compile_definitions(browser_startup_hw_tests PRIVATE CSE_TEST_HARDWARE=1)
add_executable(browser_startup_sw_tests startup_owner_tests.cpp)
target_compile_definitions(browser_startup_sw_tests PRIVATE CSE_TEST_HARDWARE=0)

foreach(target browser_action_error_tests browser_retirement_owner_tests browser_loader_owner_tests
        browser_startup_hw_tests browser_startup_sw_tests)
    add_dependencies(${target} extract_browser_ui_owners)
    target_include_directories(${target} PRIVATE "${generated}" "${sdk}/engine" "${sdk}/public")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()

foreach(case existing-add absent-remove query-preserved completion-preserved refresh-preserved)
    add_test(NAME browser_action_${case} COMMAND browser_action_error_tests ${case})
endforeach()
set(retirement_cases vgui-refusal host-refusal sys-refusal engine-refusal retry-once ready vid-failure dedicated-init)
if(APPLE)
    list(APPEND retirement_cases release-failed)
endif()
foreach(case IN LISTS retirement_cases)
    add_test(NAME browser_retirement_${case} COMMAND browser_retirement_owner_tests ${case})
endforeach()
foreach(case native-selection internal-factory internal-unload failed-initialize exact-selection
        external-factory-failure failed-post duplicate-initialize shutdown-reentry unstarted-shutdown)
    add_test(NAME browser_loader_${case} COMMAND browser_loader_owner_tests ${case})
endforeach()
foreach(renderer hw sw)
    foreach(case host-return host-flag success)
        add_test(NAME browser_startup_${renderer}_${case} COMMAND browser_startup_${renderer}_tests ${case})
    endforeach()
endforeach()
add_test(NAME browser_startup_hw_mode-failure COMMAND browser_startup_hw_tests mode-failure)

if(CSE_BROWSER_UI_PRESENTATION_TESTS)
    if(NOT EXISTS "${sdk}/GameUI/native/ServerBrowserPresentation.cpp")
        message(FATAL_ERROR "Actual presentation component is absent; this is not behavioral RED")
    endif()
    add_executable(browser_presentation_tests presentation_tests.cpp
        "${sdk}/GameUI/native/ServerBrowserPresentation.cpp" "${sdk}/GameUI/native/ServerBrowserModel.cpp")
    target_include_directories(browser_presentation_tests PRIVATE "${sdk}/GameUI/native")
    if(MSVC)
        target_compile_options(browser_presentation_tests PRIVATE /W4 /WX /EHsc)
        target_compile_definitions(browser_presentation_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(browser_presentation_tests PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(browser_presentation_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(browser_presentation_tests PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    foreach(case reachability anchor-rank sort-mutation unknown-order coalesced-sort end-clamp-sort selected-rank)
        add_test(NAME browser_presentation_${case} COMMAND browser_presentation_tests ${case})
    endforeach()
endif()

option(CSE_BROWSER_UI_CONNECTION_TESTS "Build actual browser connection owner tests" OFF)
if(CSE_BROWSER_UI_CONNECTION_TESTS)
    if(NOT EXISTS "${sdk}/GameUI/native/ServerBrowserConnection.cpp")
        message(FATAL_ERROR "Actual connection owner is absent; this is not behavioral RED")
    endif()
    add_executable(browser_connection_tests connection_tests.cpp
        "${sdk}/GameUI/native/ServerBrowserConnection.cpp" "${sdk}/GameUI/native/ServerBrowserModel.cpp")
    add_dependencies(browser_connection_tests extract_browser_ui_owners)
    target_include_directories(browser_connection_tests PRIVATE "${generated}" "${sdk}/GameUI/native")
    if(MSVC)
        target_compile_options(browser_connection_tests PRIVATE /W4 /WX /EHsc)
    else()
        target_compile_options(browser_connection_tests PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(browser_connection_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(browser_connection_tests PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    foreach(case accepted-distinct-port unrelated-acceptance intervening-change supersession validation unicode-password)
        add_test(NAME browser_connection_${case} COMMAND browser_connection_tests ${case})
    endforeach()
endif()

option(CSE_BROWSER_UI_DETAILS_TESTS "Build actual typed details owner tests" OFF)
if(CSE_BROWSER_UI_DETAILS_TESTS)
    if(NOT EXISTS "${sdk}/GameUI/native/ServerBrowserDetails.cpp")
        message(FATAL_ERROR "Actual details owner is absent; this is not behavioral RED")
    endif()
    add_executable(browser_details_tests details_tests.cpp
        "${sdk}/GameUI/native/ServerBrowserDetails.cpp" "${sdk}/GameUI/native/ServerBrowserModel.cpp")
    add_dependencies(browser_details_tests extract_browser_ui_owners)
    target_include_directories(browser_details_tests PRIVATE "${generated}" "${sdk}/GameUI/native")
    if(MSVC)
        target_compile_options(browser_details_tests PRIVATE /W4 /WX /EHsc)
    else()
        target_compile_options(browser_details_tests PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(browser_details_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(browser_details_tests PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    foreach(case synchronous-copy cancel-reentry start-reentry bounded-drain)
        add_test(NAME browser_details_${case} COMMAND browser_details_tests ${case})
    endforeach()
endif()

option(CSE_BROWSER_UI_MODEL_RETIREMENT_TESTS "Exercise real model aggregate bounded retirement" OFF)
if(CSE_BROWSER_UI_MODEL_RETIREMENT_TESTS)
    add_executable(browser_model_retirement_tests model_retirement_tests.cpp
        "${sdk}/GameUI/native/ServerBrowserModel.cpp")
    add_dependencies(browser_model_retirement_tests extract_browser_ui_owners)
    target_include_directories(browser_model_retirement_tests PRIVATE "${generated}" "${sdk}/GameUI/native")
    if(MSVC)
        target_compile_options(browser_model_retirement_tests PRIVATE /W4 /WX /EHsc)
    else()
        target_compile_options(browser_model_retirement_tests PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(browser_model_retirement_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(browser_model_retirement_tests PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    foreach(case bounded-close coalesced-replacement latest-replacement cancel-reentry stopped-request)
        add_test(NAME browser_model_retirement_${case} COMMAND browser_model_retirement_tests ${case})
    endforeach()
endif()

foreach(renderer hw sw)
    add_executable(browser_entry_${renderer}_tests entry_owner_tests.cpp)
    add_dependencies(browser_entry_${renderer}_tests extract_browser_ui_owners)
    target_include_directories(browser_entry_${renderer}_tests PRIVATE "${generated}")
    if(renderer STREQUAL "hw")
        target_compile_definitions(browser_entry_${renderer}_tests PRIVATE CSE_TEST_HARDWARE=1)
    else()
        target_compile_definitions(browser_entry_${renderer}_tests PRIVATE CSE_TEST_HARDWARE=0)
    endif()
    foreach(case invalid-entry exception reentry ready)
        add_test(NAME browser_entry_${renderer}_${case} COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/check_entry.py" "$<TARGET_FILE:browser_entry_${renderer}_tests>" ${case})
    endforeach()
    if(APPLE)
        add_test(NAME browser_entry_${renderer}_pending COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/check_entry.py" "$<TARGET_FILE:browser_entry_${renderer}_tests>" pending)
    endif()
endforeach()

foreach(renderer hw sw)
    add_executable(browser_listen_${renderer}_tests listen_owner_tests.cpp)
    add_dependencies(browser_listen_${renderer}_tests extract_browser_ui_owners)
    target_include_directories(browser_listen_${renderer}_tests PRIVATE "${generated}" "${sdk}/engine" "${sdk}/public")
    if(renderer STREQUAL "hw")
        target_compile_definitions(browser_listen_${renderer}_tests PRIVATE CSE_TEST_HARDWARE=1)
    else()
        target_compile_definitions(browser_listen_${renderer}_tests PRIVATE CSE_TEST_HARDWARE=0)
    endif()
    foreach(case filesystem-failure video-failure game-failure load-failure normal restart)
        add_test(NAME browser_listen_${renderer}_${case} COMMAND browser_listen_${renderer}_tests ${case})
    endforeach()
endforeach()

foreach(target browser_entry_hw_tests browser_entry_sw_tests browser_listen_hw_tests browser_listen_sw_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        if(CSE_BROWSER_UI_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()
