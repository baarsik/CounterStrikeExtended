# Native A3D producer, control and capability fixture

This fixture compiles the complete selected native module from generated
`Sources.cmake`, with per-TU options evaluated by the retained manifest's
`a3d_resources/prepare_policy.py`. All 21 retained TUs, five geometry sections,
and six native owners remain selected. Production arithmetic is not extracted,
rewritten, or replaced. `-fno-access-control` applies only to test observations.

Configure with the existing SDL package, then build and test serially:

```sh
cmake -S tests/native/a3d_spatial -B build/a3d-spatial-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DSDL3_DIR=/absolute/existing/SDL3/upstream
cmake --build build/a3d-spatial-release --parallel 1
ctest --test-dir build/a3d-spatial-release --parallel 1 --output-on-failure
```

Repeat with a separate Debug directory and `CMAKE_BUILD_TYPE=Debug`. Every CTest
uses process-local `SDL_AUDIO_DRIVER=dummy`. These are CPU and local-provider
checks, not physical-output, loaded-export, OEM, or game-consumer qualification.

- `consumed_fields_match_retained_state`: 45 finite bounded vectors compare the
  real native control entry with real `A2DBuffer::SetA3dSuperCtrl`, including
  frequency/rate, direct gain/delay, and HRTF state. Nine individual consumed-field
  mutations change PCM; output must be nonzero. Fixed record offsets are asserted.
- `eq_only_is_invariant` and `reflections_only_are_invariant`: populated valid
  complete records succeed, are retained byte for byte, and produce identical
  software voice state and PCM (only distinct sample allocation addresses are
  normalized). All 16 reflection slots are populated in the latter test.
- `source_control_pcm`: public source gain/pitch/position and Play feed actual
  `RenderSources`/`SubmitRenderState`, `ResManBuffer::SetA3dSuperCtrl`, `DalBufferInfo`,
  `A3dNativeBuffer`, and the retained software mixer. A labeled forwarding tap
  observes the complete record without calculating controls. The real resource
  worker is stopped and an actual destination stream is attached synchronously;
  real source samples pass through Update/Write/ConvertPcm, with destination
  bytes verified after starting from silence. CPU capture does not replace the
  sample/control algorithms or claim asynchronous device timing.
- `geometry_producer_records`: a real submitted quad creates reflection records;
  distance and actual material changes alter reflection gain/delay/EQ. This does
  not require the software mixer to render those stock-unread records.
- `reflection_slot_history`: stable tags reuse the actual slot with `dwNewSlot=0`;
  replacement tags receive a new slot indication; removed geometry disables it.
- `material_opening_transforms`: real material/face/opening constructors and
  transforms emit complete command records, including the derived material
  filter and live opening-factor pointer. The actual source consumes the opening
  command. It preserves the observed retained-source full-open/end-of-command
  solid-wall fallback; that observation is **not** proven shipping OEM behavior.
- `high_address_polygon_domains`: real polygons mapped 4 GiB apart share low32
  bits but retain distinct owned identities through face submission and source
  reflection pruning, also distinct from a numerically matching public DWORD tag.
- `occlusion_direct_gain`: real wall/material/occlusion producers attenuate the
  direct record and PCM through public `Enable(A3D_OCCLUSIONS)` and source
  `SetRenderMode`. Changing only the produced EQ term leaves PCM identical.
- `native_feature_negotiation`: actual native leaf/provider and `CA3d` APIs accept
  `A3D_DIRECT_PATH_A3D=0x08` and retained software attenuation via
  `A3D_OCCLUSIONS=0x40`, including Init and later Enable. All other 29 single-bit rendering/unknown
  requests, combined with direct, fail without initialized device state. Failed
  requests return a zero supported mask; `0x80` remains only the splash option.
  Mixed masks, zero masks, geometry defaults and disabling are checked through
  the native route. External/Windows legacy branches are preserved.

The exact stock software control evidence is pinned in the existing
`a3d-reference-analysis-v1/{findings.md,controller-checks.json,selected-evidence.json}`
artifact set: stock DLL SHA256
`f05b2eeb851b174ef2b39c4728687915648ae33780a65cdf7f0c7ce99e6a67af`,
control body `0x10001600..0x1000173a`. EQ/reflection invariance is specific to that
routine; it does not establish OEM semantics. The Task 6 field/evidence matrix is
`build/a3d-native-20261111/control-contract.md` in the owned source worktree.

Native `A3D_1ST_REFLECTIONS=0x02` and `A3D_DIRECT_PATH_GENERIC=0x20` stay
unsupported. `A3D_OCCLUSIONS=0x40` restores the retained software gain attenuation. The actual selected route is the native
super-control software path; no generic DirectSound 3D renderer is qualified.
Occlusion's full material/filter/opening meaning is not demonstrated by direct
attenuation alone, especially with unread software EQ and the observed opening
fallback. No new EQ/reflection DSP or engine audio bridge is included.
