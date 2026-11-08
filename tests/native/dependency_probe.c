#include <SDL3/SDL.h>
#include <mpg123.h>
#include <stdio.h>

int main(void) {
    const int expectedSdlVersion = SDL_VERSIONNUM(3, 4, 20);
    const int sdlVersion = SDL_GetVersion();
    int error = MPG123_OK;
    mpg123_handle *decoder = NULL;
    if (sdlVersion != expectedSdlVersion || mpg123_init() != MPG123_OK
        || !mpg123_feature2(MPG123_FEATURE_DECODE_LAYER3)) {
        fprintf(stderr, "Provider API/feature mismatch: SDL=%d, mpg123 API=%d\n", sdlVersion, MPG123_API_VERSION);
        return 1;
    }
    decoder = mpg123_new(NULL, &error);
    if (decoder == NULL || error != MPG123_OK) {
        fprintf(stderr, "Decoder handle creation failed: %d\n", error);
        mpg123_delete(decoder);
        mpg123_exit();
        return 2;
    }
    mpg123_delete(decoder);
    mpg123_exit();
    printf("SDL=%d, mpg123_api=%d, Layer3=enabled, pointer_bytes=%zu\n",
           sdlVersion, MPG123_API_VERSION, sizeof(void *));
    return 0;
}
