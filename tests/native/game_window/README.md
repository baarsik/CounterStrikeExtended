# Game window handle declarations

The actual IGame/CGame declarations are compiled for both hardware and dedicated
branches. Windows keeps its existing HWND type; native POSIX declarations use an
SDL_Window pointer. Getter and address-of-handle signatures retain pointer width.
This does not create a window or qualify the complete CGame platform backend.
