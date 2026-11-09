# Generated from launcher/launcher.vcxproj; see tools/translate_msbuild.py.
target_sources(launcher PRIVATE
  "${CSE_SDK_DIR}/launcher/launcher.cpp"
  "${CSE_SDK_DIR}/launcher/common/commandline.cpp"
  "${CSE_SDK_DIR}/launcher/public/interface.cpp"
  "${CSE_SDK_DIR}/launcher/public/registry.cpp"
)
if(WIN32)
  target_sources(launcher PRIVATE
    "${CSE_SDK_DIR}/launcher/hl.rc"
  )
endif()
