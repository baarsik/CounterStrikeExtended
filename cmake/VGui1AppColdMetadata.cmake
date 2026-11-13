# Unselected opt-in helper. No target invokes this from production manifests.
include_guard(GLOBAL)
function(cse_add_vgui1_app_cold_metadata target role contexts evidence_directory)
    if(NOT APPLE OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT TARGET "${target}")
        message(FATAL_ERROR "AppCold metadata requires an existing Apple64 owner")
    endif()
    if(NOT role MATCHES "^(offer|requirement)$")
        message(FATAL_ERROR "AppCold dependency-edge role must be offer or requirement")
    endif()
    get_target_property(prior "${target}" CSE_APP_COLD_METADATA)
    if(prior)
        message(FATAL_ERROR "Only one AppCold dependency-edge record belongs to an image")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_filename_component(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(publisher "${root}/src/common/vgui1_app_cold_publisher.cpp")
    # Always refresh the actual owning recipes and full include closure. No
    # generated header is force-included into App.cpp/vgui_intwrap.cpp probes.
    add_custom_target("${target}_app_cold_evidence"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tools/vgui1_app_cold_evidence.py" generate
            --schema "${root}/tools/vgui1_app_cold_evidence_schema.json" --contexts "${contexts}"
            --foundation "${root}/src/public/cse_vgui1_app_cold_abi.h"
            --output "${evidence_directory}" --metadata
        BYPRODUCTS "${evidence_directory}/metadata.h" "${evidence_directory}/metadata.stamp"
        VERBATIM)
    add_dependencies("${target}" "${target}_app_cold_evidence")
    # A target-private generated wrapper prevents directory-scoped source
    # definitions for offer/requirement from contaminating another image.
    string(TOUPPER "${role}" role_upper)
    set(wrapper "${CMAKE_CURRENT_BINARY_DIR}/${target}-app-cold-publisher.cpp")
    file(GENERATE OUTPUT "${wrapper}" CONTENT
        "#define CSE_VGUI1_APP_COLD_${role_upper} 1\n#define CSE_VGUI1_APP_COLD_GENERATED_HEADER \"${evidence_directory}/metadata.h\"\n#include \"${publisher}\"\n")
    target_sources("${target}" PRIVATE "${wrapper}")
    set_property(TARGET "${target}" PROPERTY CSE_APP_COLD_METADATA YES)
endfunction()
