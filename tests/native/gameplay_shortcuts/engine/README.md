These macOS-only fixtures execute both complete `CGame::SleepUntilInput` bodies,
the current UI-context predicate, native window owner and Cocoa adapter. SDL,
engine destinations and native setter results are controlled seams; no window,
provider callback, keyboard event or game is opened/injected here.

The lifecycle targets also execute the complete selected `RunListenServer` and
`CGame::Shutdown` bodies, deferred cleanup phases, actual native bootstrap TU and
actual launcher `Session::Close` body. The launcher image-owner calls are counted
seams, not a real dyld unload. `pthread_main_np` and the worker thread are real.

Coverage includes committed gameplay versus chat/menu/console/custom cursor,
current focus/window identity, pending/failed UI requests, both VGUI callers in
the retained mouse suite, unchanged Control/Space delivery, no steady setter
writes, all native-dispatch refresh sites, suspension/resumption, native failure,
nested invalidation, failed startup, quit before engine unload, retryable later
shutdown, replacement admission and recursive cleanup. Audio-extra-update and
WebM dispatch blocks run with controlled audio/playback seams; this is not audio
or renderer qualification. Existing client fixtures cover accepted motion,
raw-off capture, cutoff/first delta and mouse arithmetic. Command/binding/wait
owners are byte-pinned unchanged, not simulated as a new command implementation.

Run from the repository root, with the retained local SDL dependency cache:

```sh
python3 tests/native/gameplay_shortcuts/engine/run_validation.py \
  --repo "$PWD" --output "$PWD/build/gameplay-shortcuts-validation"
```

The output directory must not already exist. This runs Release, Debug and
ASan/UBSan suites, at most two compilers. Every command, exit, extracted body,
compile database, source hash and unchanged control is retained. Individual
CMake projects accept `CSE_SDL_INCLUDE`, `CSE_SDL_CONFIG`, and their named sanitizer
options for another cache location.

`extract.py` preserves complete selected production functions and pins their
sources/bodies. The WebM import is explicitly its local event-dispatch block,
not the decoder or renderer. The initial baseline RED is behavioral: both real
event loops reach their first native pump without the gameplay policy enabled.

`../build_engine_packet.py` replays qualified engine/launcher owning commands and
full link closures against a supplied Task 1 SDL packet. It preserves the reviewed
genuine-main runtime/image-owner objects, signs the three candidate images and
regenerates the launcher cohort hashes. It never stages or launches a game.
