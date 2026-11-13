# Headless fullscreen refresh policy

This fixture compiles the complete `CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen`
body from production `sys_getmodes.cpp`. SDL display queries and transitions are injected
leaf functions; no SDL library, video subsystem, window or physical display is opened.

The scaled-FBO transition requests a supported nominal rate at desktop pixel dimensions.
Cases cover desktop/no request, substituted refresh/resolution rejection, unavailable
mode information, rejected mode application, windowed exclusion, and preserving the
already-selected GL fallback mode. The fallback flag is supplied at this boundary;
this fixture does not execute GL/FBO allocation or qualify a monitor/driver.

```sh
cmake -S tests/native/fullscreen_refresh -B build/fullscreen-refresh -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/fullscreen-refresh --parallel 2
ctest --test-dir build/fullscreen-refresh --output-on-failure
```

Use `-DSANITIZERS=ON` for AddressSanitizer and UndefinedBehaviorSanitizer. The project
uses the host's real platform preprocessor branch; the restored native policy is
qualified on macOS, while Windows retains its existing refresh handling.
