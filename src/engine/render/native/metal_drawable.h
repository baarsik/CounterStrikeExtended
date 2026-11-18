#pragma once

struct SDL_Window;

namespace cse {
namespace render {
int MetalDrawableCount(SDL_Window* window);
bool SetMetalDrawableCount(SDL_Window* window, int count);
}
}
