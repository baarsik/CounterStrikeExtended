# The pinned SDL 3.4.20 Metal backend exposes a non-null swapchain wrapper even
# when CAMetalLayer returns no drawable. Its submit path then presents nil.
# Release the previous drawable texture before waiting for another one, and
# leave the SDL output null when the layer cannot supply a drawable.
set(metal_backend "${source_root}/SDL3-3.4.20/src/gpu/metal/SDL_gpu_metal.m")
file(READ "${metal_backend}" metal_source)
set(before [=[
        // Get the drawable and its underlying texture
        windowData->drawable = [windowData->layer nextDrawable];
        windowData->texture.handle = [windowData->drawable texture];

        // Set up presentation
]=])
set(after [=[
        // Release the previous drawable's texture before waiting for a new drawable.
        windowData->texture.handle = nil;
        windowData->drawable = [windowData->layer nextDrawable];
        if (windowData->drawable == nil) {
            return true;
        }
        windowData->texture.handle = [windowData->drawable texture];

        // Set up presentation
]=])
string(FIND "${metal_source}" "${before}" before_offset)
if(before_offset EQUAL -1)
  string(FIND "${metal_source}" "${after}" after_offset)
  if(after_offset EQUAL -1)
    message(FATAL_ERROR "Pinned SDL Metal acquisition no longer matches the qualified patch")
  endif()
else()
  string(REPLACE "${before}" "${after}" metal_source "${metal_source}")
  file(WRITE "${metal_backend}" "${metal_source}")
endif()
