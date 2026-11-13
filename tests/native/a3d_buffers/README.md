# Native A3D software buffers

This fixture compiles the real `native_buffer.cpp`, `native_mix.cpp`, `a2d.cpp`,
`a2dbuffer.cpp` and platform allocation owner. It opens no device, initializes no
SDL subsystem, and never links `a3d_output/fixture_mixer.cpp`. Subject TU options
come from the retained manifest through the same translator used by the owning
graph. The only extracted body is the exact integer multiply leaf from A3dGeom.

Configure a new owning build directory with the available SDL3 config:

```sh
cmake -S tests/native/a3d_buffers -B build/a3d-buffers -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DSDL3_DIR=/path/to/SDL3/config
cmake --build build/a3d-buffers --parallel 1
ctest --test-dir build/a3d-buffers --parallel 1 --output-on-failure
```

Use `-DA3D_SANITIZERS=ON` for ASan/UBSan. The fixture requires POSIX/AppleClang;
the existing float-state fixture supplies the separate Windows reference route.

The buffer suites exercise real copied format/control data, full consumed
DirectSound and 3D views, canonical identity, shared duplicate storage and
independent cursors, split locks, bounded byte projections, parent/child release,
and simultaneous rendering/release. The mixer tests exercise 0/1/2/3/4096/larger
requests with the actual 4096-frame split, logical input sizes1/2/3/64 for all four
PCM layouts, multivoice accumulation, independent instances, and exact
hand-derived PCM from the retained HRTF coefficients.

`retained_pcm_receipts` compares all32 historical Windows receipt files exactly,
using the actual production mixer TU without the older host-long adapter.
Those historical cases retain their documented deterministic guard samples and
filtered-component coverage; they are separate from native-owned logical-input
safety tests. Native Render retains the DAL_A2D `AR2Dproc(..., FALSE)` call and
does not enable the filtered path. The only logical-end extension in native
storage is one owned silence byte consumed by the retained mono8 boundary
reader. Callers upload exactly the logical bytes and cannot lock the extension.

Low-level compatibility is deliberate: A2D's super-control flags and unread
EQ/reflection/reserved fields do not alter PCM. Volume/pan/mute bookkeeping is
not consumed by the retained mix loop; frequency is consumed on the next
super-control submission. These successful compatibility operations are tested
separately from unsupported methods, which return failure. Stored 3D parameters
are consumed by the higher-level source producer; this fixture does not invent
an independent DirectSound spatial renderer. Deferred 3D commits remain
unsupported. No native feature bit or OEM/device/module qualification is claimed.

## Ownership for later consumers

A3dNativeMixer creates buffers with one caller reference. Its intrusive list is
borrowed under A3dNativeMixLock; final buffer Release removes the list entry
before freeing the object. Mixer destruction detaches/stops retained buffers,
which keep sample/format/control state until their final Release; Play then
returns E_ABORT. Provider/output shutdown must join the render worker before
destroying its mixer. Native owner classes are noncopyable where they own lists.

DuplicateBuffer accepts only a currently attached buffer from the same mixer,
without calling foreign QI. Each duplicate retains the sample allocation and
snapshots the effective kernel/control state, including an initial voice with no
submitted record or a completed rate ramp. The pending frequency scalar is copied
separately and takes effect only on a later explicit control submission. Duplication
does not replay a zero-initialized or last-submitted record. Its independent
playhead starts at zero, using the same cursor/phase/delay reset as an explicit seek.
Fallible initialization finishes while borrowing the source storage under the mix
lock, before acquiring the duplicate's shared reference. A fixture-only one-shot
nothrow-allocation override exercises actual allocation failure and surviving
shared-storage playback; production has no fault hook. Lock grants one shared-storage writer;
Render skips that storage until validated Unlock. The caller must retain its
buffer while using its borrowed Lock pointers. Control submission copies the
entire record and consumes only the stock direct-channel fields immediately.

The static mix mutex serializes list/control/PCM work across all instances and
protects retained kernel scratch. No foreign interface calls occur under it.
Task5 must attach typed event registrations and crossing/stop dispatch to these
actual owners; this task exposes no fake notify capability, HANDLE shim or
notification success stub. Cursor projection alone is not a complete multiple
wrap notification history. Module unload counts/admission remain later work.

Native render admission also checks the retained integer rate transition before
mutating a block. A one-frame request cannot absorb every physically meaningful
new delay: stock's negative-rate branch is a debug trap. Such a block now returns
E_INVALIDARG, leaving its output and voices unchanged; retrying with a sufficiently
large block uses the unchanged stock arithmetic. Sample/tracker projections that
would exceed signed32 storage likewise fail before entering the kernel. This is
bounded native admission, not a change to interpolation or Windows behavior.

`qualified_conversion_edges` pins22 genuine x86/x64 Release/Debug conversion
outputs, including NaN/infinities, signed32 edges, ties, clipping and signed zero.
Every output prefix length0..22 is checked with adjacent sentinels; native control
state is preserved and the fixture restores the caller's complete FP environment.
The immutable capture receipt SHA256 is
`4038be1435b8548c16941d19964d7d0c188490672aa134c1b20e56231af65f62`;
`../a3d_float_state/fixtures/conversion-manifest.json` records exact provenance.


The output integration cases compile `native_output.cpp` with the same actual
buffer/mixer/kernel objects and use real, unbound SDL conversion streams. They
compare a gain/pitch/delay transition against one retained 2048-frame invocation,
check that failed puts retry the identical PCM, and verify that transient output
and terminal render errors reach existing buffer GetStatus/Play and mixer
CreateBuffer/DuplicateBuffer paths. Successful retry restores those operations.
All output cases explicitly use `SDL_AUDIO_DRIVER=dummy`; no physical device is
opened and no game is launched.
