# Native music focus regression

This fixture compiles both complete actual CGame::AppActivate bodies, the actual
owning cd.h declarations and complete controller method bodies, plus unchanged
production thread.cpp. The extractor pins original file/body hashes and emits
#line provenance. Its optional helper spans support a RED against the original
source; no substitute focus decision logic is supplied. ICDAudio/vtable and
CCDAudio layout checks belong to the associated qualification receipt.

Named backend/filesystem/UI seams are doubles. No actual audio device, decoder,
asset, microphone, game, UI, global preference or system route is touched. The
backend scripts pause/resume/init/open/start failures and records stream/position
and attempts. These tests prove controller dispatch/state decisions, not audible
playback, decoder fidelity or physical audio-device restoration.

The actual bounded CD queue, pthread worker, admission results, FIFO batches and
shutdown/join run unchanged. A source-declared unrelated disk-info callback serves
only as an atomic queue barrier; it does not model focus logic. Holding the actual
recursive command mutex lets a test fill all ten slots, observe rejection, and
verify Frame retries focus admission. An in-flight backend pause can block the
worker while actual Shutdown rejects new work and waits for the callback before
closing playback. Failed subprocess cases exit without unloading running code;
every passing case explicitly joins/closes the worker.

Cases cover ordinary pause/resume without reopening/rewinding, preexisting/manual
pauses, stopped/finished/disabled controllers, repeated admitted focus and manual
Pause/Stop/new Play ordering, admission saturation, failed pause/resume with later
retry, numeric/file explicit suspension init/open/start failures with metadata
and clock preservation, manual cancellation, Stop after failed restoration, and
queued/in-flight shutdown. Explicit released-stream restoration retains the
existing reopen semantics; it does not pretend to recover a decoder position
already discarded by MP3_Suspend_Audio. Ordinary focus pause keeps that stream.

Configure with CSE_MUSIC_SOURCE_ROOT pointing at the pinned complete source (or a
RED snapshot containing the six controller/window/worker source/header files),
then build and run CTest. Admitted focus jobs carry their active snapshot in the
existing FIFO; only rejected requests coalesce into latest desired state for
Frame admission retry. The gain→Play→loss regression checks actual queue order. CSE_MUSIC_SANITIZERS=ON adds ASan/UBSan with no recovery.
The fixture is Apple-only; unchanged Windows compiled behavior is separately
qualified by the controller under the actual owning profiles.
