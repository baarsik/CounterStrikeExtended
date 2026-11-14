# Engine image consistency checks

Run from the repository root after building the pinned WebP provider:

```sh
python3 tests/native/image_consistency/run.py \
    --evidence build/image-consistency-tests \
    --webp-library build/webp-codec-debug/lib/Debug/webp/md/libcse_webp_md.a \
    --webp-include build/webp-codec-debug/dependencies/WebP/source/libwebp-1.6.0/src
```

The runner compiles both renderers with C++14, warnings as errors, AddressSanitizer
and UndefinedBehaviorSanitizer. Each executable and its log stay in the evidence
folder. No server or game process is launched.

The policy/cache implementation is taken directly from `image_consistency.cpp`.
The fixtures also extract the production `CL_ParseConsistencyInfo`,
`CL_ClearClientState`, resource-list cleanup, and `CL_RegisterResources` functions,
plus each renderer's real image loader, file texture loader, texture binding,
RGBA/BGRA upload methods, surface destructor, and texture deletion entry point.
Filesystem reads and GPU/software storage are isolated boundaries. Packet bit
values are scripted, while serialization, model loading, and unrelated signon
operations are replaced with small boundaries. The serialization boundary
observes pixels to verify refresh happens before the consistency response.

Distinct red WebP, blue TGA, green case-variant files, and an opaque black failure
image exercise:

- TGA-only consistency blocking unchecked WebP for legacy, extensionless and
  explicit WebP requests; checked WebP and ordinary sidecars remain available.
- WebP-only requirements refusing unchecked stock TGA fallback.
- Checked TGA filenames containing WebP payloads.
- Server-declared case and slash spelling, including a missing checked case
  variant beside a differently colored lowercase file.
- Sound/model-bound checks and unrelated resources leaving image policy alone.
- Cached and already-bound IDs refreshed before signon; reconnecting to an
  unrestricted server restores optional replacements.
- WebP downloads arriving after HUD initialization, including demo playback.
- Missing protected files suppressing unchecked BMP/stale image fallback and
  retaining provenance for a successful retry.
- Independent GL binding, renderer binding cache (including its unknown sentinel),
  VGUI selected ID/null selection, and texture-record relocation during refresh.
- Owned request strings, programmatic
  RGBA/BGRA overwrites, texture deletion, and owner destruction.

These fixtures verify loader/cache behavior. Full Windows builds and gameplay or
visual acceptance remain separate validation.
