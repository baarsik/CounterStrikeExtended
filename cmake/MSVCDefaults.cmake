# Repository targets supply their own configuration policy. In particular,
# CMake's default Release /DNDEBUG and Debug /RTC1 must not leak into targets.
# This hook sets initial defaults only; explicit user cache overrides remain visible.
foreach(lang C CXX OBJCXX)
  set(CMAKE_${lang}_FLAGS_INIT "")
  foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_${lang}_FLAGS_${config}_INIT "")
  endforeach()
endforeach()
foreach(kind EXE SHARED MODULE)
  set(CMAKE_${kind}_LINKER_FLAGS_INIT "")
  foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_${kind}_LINKER_FLAGS_${config}_INIT "")
  endforeach()
endforeach()
