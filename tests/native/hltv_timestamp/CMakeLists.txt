cmake_minimum_required(VERSION 3.25)
project(cse_hltv_timestamp_fixture LANGUAGES CXX)

if(NOT MSVC OR NOT WIN32)
    message(FATAL_ERROR "This fixture qualifies the retained Windows MSVC writer expression")
endif()
if(NOT CSE_HLTV_V11_SOURCE)
    message(FATAL_ERROR "Pass the immutable V11 producing source directory")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_fixture.py"
            --source "${CSE_HLTV_V11_SOURCE}" --destination "${generated}"
    COMMAND_ERROR_IS_FATAL ANY
)

add_executable(hltv_timestamp timestamp_fixture.cpp writer_expressions.cpp)
target_compile_features(hltv_timestamp PRIVATE cxx_std_17)
target_include_directories(hltv_timestamp PRIVATE "${generated}")
set_property(TARGET hltv_timestamp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
target_compile_options(hltv_timestamp PRIVATE /W4 /WX /EHsc /GR /fp:precise
    "$<$<CONFIG:Debug>:/Od>" "$<$<CONFIG:Debug>:/RTC1>" "$<$<CONFIG:Release>:/O2>")
