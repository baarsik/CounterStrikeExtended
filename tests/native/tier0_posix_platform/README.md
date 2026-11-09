# POSIX platform time and argument boundaries

The complete actual `tier0/platform_posix.cpp` is replayed with its generated
Mac configuration command. Source/include/output paths and dependency metadata
are the only normal changes; sanitizer builds add ASan/UBSan. The normal object
uses real `gettimeofday`. The separate time-control object renames that one API
to a typed fixture binding. Exact production VCR prefix declarations are extracted
for a hook binding; VCR file recording/playback is not executed or qualified.

```sh
cmake -S tests/native/tier0_posix_platform -B build/tier0-posix-platform-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_POSIX_PLATFORM_DATABASE=/absolute/path/to/macos-runtime-policy-debug/compile_commands.json
cmake --build build/tier0-posix-platform-debug --parallel 2
ctest --test-dir build/tier0-posix-platform-debug --output-on-failure -V
```

Use the matching Release database for Release and
`CSE_POSIX_PLATFORM_SANITIZERS=ON` for the full actual TU and fixture.

Each time function retains its independent zero-sentinel epoch, gettimeofday
arithmetic and first-call behavior. Only the private base storage changes to the
actual `timeval::tv_sec` type. FloatTime still bypasses the VCR hook on a first
call, including repeated base-zero calls. MSTime still truncates fractional
seconds on its first call, normally returning zero; later calls preserve the
existing millisecond truncation and unsigned 32-bit modulo encoding. There is
no monotonic-clock conversion or general concurrency/failure-policy change.

The two existing argument appends now reserve their terminating null byte.
Joining remains unquoted, retains a trailing space when it fits, and returns the
first 2047 content bytes when truncated. The backing array remains 2048 bytes.
No argument-parser redesign or positive-argc null-argument extension is made.

Seven cases cover exact command bytes, empty arguments, null argv with argc zero,
2046/2047-byte boundaries, long and multiple arguments, actual clock availability,
an epoch beyond 2038, single-microsecond binary64 encoding, independent bases,
negative and rollover millisecond encodings, record/playback hook dispatch,
first-call hook bypass and both zero-sentinel quirks. Floating outputs are checked
bit-for-bit; there is no tolerance. Four defined old-source scenarios are also
compared byte-for-byte with the preserved reference executable.

The strict original whole-TU commands fail on two native-seconds assignments and
two unsafe strncat bounds. A separate immutable-reference mode requires an
explicit archived source path. It only demotes the visible observed
`shorten-64-to-32` and `strncat-size` warnings, leaving their diagnostics in the
log; this is never enabled for production input. That source reproduces the
high-epoch delta failure and an ASan global-buffer-overflow at the trailing-space
append for a 2047-byte argument. All negative sources, commands and binaries are
preserved in the qualification packet.

Despite its filename, this source is active in the retained Windows manifest
and contains a separate Windows branch. The fixture records actual manifest
membership and unchanged Windows-selected source bytes/tokens. Prior actual V11
Win32/x64 Release/Debug build receipts are referenced separately. No new MSVC
invocation, VM action, module/provider launch or game acceptance occurs here.
