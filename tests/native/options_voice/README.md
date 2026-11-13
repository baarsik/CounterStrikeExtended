# Sound options resource compatibility

`options_voice_tests.cpp` compiles the complete production `OptionsSubVoice.cpp`,
`OptionsSubAudio.cpp`, `CvarSlider.cpp`, `CvarToggleCheckButton.cpp`,
`LabeledCommandComboBox.cpp` and `ModInfo.cpp` against actual VGUI controls.
The shared builder is `tests/native/server_browser_ui/build_vgui_component.py`
with `--driver options_voice`. It uses the shared generated model/geometry helpers.

Run the resulting `vgui_tests` executable with these arguments:

```text
vgui-image filesystem-image generated-resources output-directory language platform-directory official-valve-directory
```

The official, read-only Valve resources must still contain their visible
`MilesVoiceLabel` with `#GameUI_Miles_Voice` and `MilesAudioLabel` with
`#GameUI_Miles_Audio`. The fixture checks that both controls are retired by the
actual pages, including repeated resource loads and scheme reapplication.
Existing audio sliders and sound-quality selection apply through the actual
owner. Existing voice controls remain visible, volume and boost changes reach
the controlled voice API, microphone testing still starts/stops, and unavailable
voice support still disables its controls. Engine cvars and voice functions are
typed in-memory providers. Registry reads/writes use the shared private provider;
there is no game launch or physical microphone access.

Use the same admitted VGUI/filesystem/SDL dependencies, dummy SDL drivers and
private preferences environment as the shared actual-controls fixtures.
