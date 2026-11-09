# Generated from tier0/tier0.vcxproj; see tools/translate_msbuild.py.
target_sources(tier0 PRIVATE
  "${CSE_SDK_DIR}/tier0/assert_dialog.cpp"
  "${CSE_SDK_DIR}/tier0/cpu.cpp"
  "${CSE_SDK_DIR}/tier0/cpu_posix.cpp"
  "${CSE_SDK_DIR}/tier0/dbg.cpp"
  "${CSE_SDK_DIR}/tier0/fasttimer.cpp"
  "${CSE_SDK_DIR}/tier0/mem.cpp"
  "${CSE_SDK_DIR}/tier0/mem_helpers.cpp"
  "${CSE_SDK_DIR}/tier0/memstd.cpp"
  "${CSE_SDK_DIR}/tier0/minidump.cpp"
  "${CSE_SDK_DIR}/tier0/platform_posix.cpp"
  "${CSE_SDK_DIR}/tier0/pme_posix.cpp"
  "${CSE_SDK_DIR}/tier0/pme.cpp"
  "${CSE_SDK_DIR}/tier0/testthread.cpp"
  "${CSE_SDK_DIR}/tier0/thread.cpp"
  "${CSE_SDK_DIR}/tier0/threadtools.cpp"
  "${CSE_SDK_DIR}/tier0/vcrmode_posix.cpp"
  "${CSE_SDK_DIR}/tier0/vprof.cpp"
  "${CSE_SDK_DIR}/tier0/validator.cpp"
  "${CSE_SDK_DIR}/tier0/valobject.cpp"
)
if(WIN32)
  target_sources(tier0 PRIVATE
    "${CSE_SDK_DIR}/tier0/assert_dialog.rc"
  )
endif()
