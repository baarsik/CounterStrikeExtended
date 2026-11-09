# Panel text code units

The complete production Panel.cpp is compiled separately with its actual runtime
commands. This standalone fixture extracts the unchanged InternalKeyTyped method
and checks its retained UTF-16 unit conversion, build-mode filtering and unchanged
parent-message integer forwarding. Native wchar_t storage can be wider without
turning this legacy callback into a different scalar-code-point API.

```sh
cmake -S tests/native/panel_text -B build/panel-text -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/panel-text
ctest --test-dir build/panel-text --output-on-failure
```

Use Debug and CSE_ENABLE_SANITIZERS=ON for the instrumented case. This is not
full Unicode rendering, IME, input-provider or clipboard acceptance.

The connected text-input cases also compile the actual SDL text-event arm and
actual UTF8 decoder, forwarding its callback to the actual Panel method. BMP,
supplementary, mixed, CESU8 and invalid-input controls check exact UTF16 unit
sequences for direct and parent dispatch. Native text events must split a
supplementary scalar before Panel's retained 16-bit-unit boundary. This does not
qualify complete IME, font rendering or localization conversion.
