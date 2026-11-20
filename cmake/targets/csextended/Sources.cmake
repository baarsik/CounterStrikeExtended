target_sources(csextended PRIVATE
  "${CSE_ROOT}/src/amxx/csextended/module.cpp"
  "${CSE_ROOT}/src/amxx/csextended/stats/stats.cpp"
  "${CSE_ROOT}/src/amxx/csextended/stats/stats_state.cpp"
  "${CSE_ROOT}/src/amxx/csextended/stats/natives.cpp"
  "${CSE_ROOT}/src/amxx/csextended/stats/legacy_messages.cpp"
  "${CSE_ROOT}/src/amxx/csextended/forms/forms.cpp"
  "${CSE_ROOT}/src/amxx/csextended/forms/session.cpp"
  "${CSE_ROOT}/src/public/cse_server_forms.cpp"
  "${CSE_ROOT}/src/public/cse_server_forms_xaml.cpp"
  "${CSE_ROOT}/src/public/cse_server_forms_image.cpp"
  "${CSE_ROOT}/src/common/image_codec.cpp"
  "${CSE_AMXX_SDK}/amxmodx/public/sdk/amxxmodule.cpp")
