# Authoritative CMake build definition; edit this file directly.
target_sources(particleman PRIVATE
  "${CSE_SDK_DIR}/particleman/pman_frustum.cpp"
  "${CSE_SDK_DIR}/particleman/pman_main.cpp"
  "${CSE_SDK_DIR}/particleman/pman_math.cpp"
  "${CSE_SDK_DIR}/particleman/pman_particlemem.cpp"
  "${CSE_SDK_DIR}/particleman/pman_triangleeffect.cpp"
  "${CSE_SDK_DIR}/public/interface.cpp"
)
