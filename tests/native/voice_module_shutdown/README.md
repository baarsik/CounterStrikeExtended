Extracts the actual recorder-release and owned audio-module-close functions.
It uses the real `IVoiceRecord` declaration with controlled recorder, worker,
music and effects objects. No device, provider, module or game executes.

Cases require recorder/mixer clear-before-Release during reentry, one release across repeated
cleanup, both owners closed before independent effects shutdown, no cleanup past a
failed worker join, and propagation of a controlled close exception to the
outer fail-closed guard. The real SDL callback/quiescence fixture is separate in
`tests/native/voice_capture`; audio-module unload has its own provider fixture.

```sh
cmake -S tests/native/voice_module_shutdown -B build/voice-module-shutdown -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/voice-module-shutdown
ctest --test-dir build/voice-module-shutdown --output-on-failure
```

Use `-DCSE_VOICE_SHUTDOWN_SANITIZERS=ON` for ASan/UBSan. This component does not
qualify codec, Steam voice, microphone, mixer controls or full-engine shutdown.
