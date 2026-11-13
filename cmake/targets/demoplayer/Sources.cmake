# Authoritative CMake build definition; edit this file directly.
target_sources(demoplayer PRIVATE
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/DemoPlayer.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/BitBuffer.cpp"
  "${CSE_SDK_DIR}/HLTV/common/byteorder.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/common.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/DirectorCmd.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/HLTV/common/mathlib.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/common/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/common/ObjectDictionary.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/common/TokenLine.cpp"
  "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/public/interface.cpp"
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_property(SOURCE "${CSE_SDK_DIR}/HLTV/DemoPlayer/private/common/BaseSystemModule.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(demoplayer PRIVATE "${CSE_SDK_DIR}/common/native/BaseSystemModule.cpp")
endif()
