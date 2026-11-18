cse_add_amxx_module(csextended)
include("${CMAKE_CURRENT_LIST_DIR}/Sources.cmake")
cse_link_webp(csextended)

set(deploy "${CSE_ROOT}/src/amxx/csextended/deploy")
set(configs "${CSE_AMXX_PACKAGE}/configs")
set(scripting "${CSE_AMXX_PACKAGE}/scripting")
set(data "${CSE_AMXX_PACKAGE}/data/csextended/forms")
set(modules "${CSE_AMXX_PACKAGE}/modules")

add_custom_target(csextended_package_assets
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${configs}" "${scripting}/include" "${data}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CSE_AMXX_PACKAGE}/data/cse_forms"
  COMMAND "${CMAKE_COMMAND}" -E rm -f
    "${modules}/cse_forms_amxx.dll" "${modules}/cse_extstats_amxx.dll"
    "${modules}/cse_forms_amxx_i386.so" "${modules}/cse_extstats_amxx_i386.so"
    "${configs}/cse_extstats.cfg" "${configs}/cse_forms.cfg"
    "${scripting}/include/cse_extstats.inc" "${scripting}/include/cse_forms.inc"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${deploy}/csextended.cfg" "${configs}/"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${deploy}/csextended.inc" "${scripting}/include/"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${deploy}/examples/stats.sma" "${scripting}/example.sma"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${deploy}/examples/forms.sma" "${scripting}/cse_forms_example.sma"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${deploy}/forms/showcase.form" "${deploy}/forms/showcase-mark.webp"
    "${deploy}/forms/showcase-session.rgba" "${data}/"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CSE_ROOT}/docs/extended-stats.md" "${CSE_ROOT}/docs/server-forms.md" "${CMAKE_BINARY_DIR}/package/"
  VERBATIM COMMENT "Stage csextended module package assets")
add_dependencies(csextended csextended_package_assets)
