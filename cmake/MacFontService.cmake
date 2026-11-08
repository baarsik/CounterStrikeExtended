include_guard(GLOBAL)

# Source membership comes from the sole owning retained manifest. All consumers
# share this dylib: CoreText process registration does not provide ownership counts.
function(cse_define_mac_font_registration_service source)
  if(NOT APPLE)
    message(FATAL_ERROR "CoreText font registration requires a macOS target")
  endif()
  if(TARGET cse_mac_font_registration)
    message(FATAL_ERROR "The macOS font registration service must have one source owner")
  endif()
  add_library(cse_mac_font_registration SHARED "${source}")
  target_compile_features(cse_mac_font_registration PRIVATE cxx_std_17)
  target_link_libraries(cse_mac_font_registration PRIVATE "-framework CoreText" "-framework CoreFoundation")
  set_target_properties(cse_mac_font_registration PROPERTIES
    OUTPUT_NAME cse_font_registration
    CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    INSTALL_NAME_DIR "@rpath" BUILD_WITH_INSTALL_NAME_DIR YES)
  if(DEFINED CSE_OUTPUT_DIRECTORY)
    set_target_properties(cse_mac_font_registration PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${CSE_OUTPUT_DIRECTORY}")
  endif()
endfunction()
