// Reproduce Windows SDK min/max macros after the standard library and GL types.
#include "studio_mesh_batch.h"
#include <SDL3/SDL_opengl.h>
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "studio_mesh_gl.h"
#undef max
int main() {
    return cse::studio::SubmitIndexed({}, {}, nullptr, true) ? 1 : 0;
}
