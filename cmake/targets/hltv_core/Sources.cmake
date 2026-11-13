# Authoritative CMake build definition; edit this file directly.
target_sources(hltv_core PRIVATE
  "${CSE_SDK_DIR}/HLTV/Core/BSPModel.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Core.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/delta.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/NetSocket.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Network.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/Server.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/World.cpp"
  "${CSE_SDK_DIR}/HLTV/common/BitBuffer.cpp"
  "${CSE_SDK_DIR}/HLTV/common/byteorder.cpp"
  "${CSE_SDK_DIR}/HLTV/common/common.cpp"
  "${CSE_SDK_DIR}/HLTV/common/DemoFile.cpp"
  "${CSE_SDK_DIR}/HLTV/common/DirectorCmd.cpp"
  "${CSE_SDK_DIR}/HLTV/common/InfoString.cpp"
  "${CSE_SDK_DIR}/HLTV/common/mathlib.cpp"
  "${CSE_SDK_DIR}/HLTV/common/md5.cpp"
  "${CSE_SDK_DIR}/HLTV/common/munge.cpp"
  "${CSE_SDK_DIR}/HLTV/common/NetAddress.cpp"
  "${CSE_SDK_DIR}/HLTV/common/NetChannel.cpp"
  "${CSE_SDK_DIR}/HLTV/common/random.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/common/BaseSystemModule.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/common/ObjectDictionary.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/public/interface.cpp"
  "${CSE_SDK_DIR}/common/ObjectList.cpp"
  "${CSE_SDK_DIR}/common/TokenLine.cpp"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/blocksort.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/bzlib.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/compress.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/crctable.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/decompress.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/huffman.c"
  "${CSE_SDK_DIR}/HLTV/Core/private/utils/bzip2/randtable.c"
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_property(SOURCE "${CSE_SDK_DIR}/HLTV/Core/private/common/BaseSystemModule.cpp" PROPERTY HEADER_FILE_ONLY TRUE)
  target_sources(hltv_core PRIVATE "${CSE_SDK_DIR}/common/native/BaseSystemModule.cpp")
endif()
