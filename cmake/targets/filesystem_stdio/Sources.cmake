# Authoritative CMake build definition; edit this file directly.
target_sources(filesystem_stdio PRIVATE
  "${CSE_SDK_DIR}/public/characterset.cpp"
  "${CSE_SDK_DIR}/filesystem/BaseFileSystem.cpp"
  "${CSE_SDK_DIR}/filesystem/localized_filesystem.cpp"
  "${CSE_SDK_DIR}/filesystem/dbg.cpp"
  "${CSE_SDK_DIR}/filesystem/FileSystem_Stdio.cpp"
  "${CSE_SDK_DIR}/filesystem/interface.cpp"
  "${CSE_SDK_DIR}/filesystem/utlsymbol.cpp"
)
if(APPLE)
  target_sources(filesystem_stdio PRIVATE "${CSE_SDK_DIR}/filesystem/linux_support.cpp")
  target_sources(filesystem_stdio PRIVATE "${CSE_SDK_DIR}/filesystem/pathmatch.cpp")
endif()
