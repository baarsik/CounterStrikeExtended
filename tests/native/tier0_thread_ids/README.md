# macOS logical thread IDs

This compiles the complete actual `tier0/thread.cpp` using its generated runtime
command. The replay changes source/include/output paths and adds a dependency
file; sanitizer runs add ASan/UBSan. It does not build or launch a game or module
provider. No root build policy or manifest is changed.

```sh
cmake -S tests/native/tier0_thread_ids -B build/tier0-thread-ids-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCSE_THREAD_ID_DATABASE=/absolute/path/to/macos-runtime-policy-debug/compile_commands.json
cmake --build build/tier0-thread-ids-debug --parallel 2
ctest --test-dir build/tier0-thread-ids-debug --output-on-failure -V
```

Use the matching Release database for Release. Set `CSE_THREAD_ID_SANITIZERS=ON`
to instrument the actual translation unit and fixture.

The Mac branch uses the native `mach_port_t` returned by
`pthread_mach_thread_np(pthread_self())`. Static assertions require this to
already be the unsigned 32-bit logical ID type. It never converts a pthread
pointer or narrows a 64-bit OS ID. The native API is declared in the installed
Apple SDK; [Apple's scheduler guide](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html)
uses this thread-port query, and the pinned
[Apple libpthread implementation](https://github.com/apple-oss-distributions/libpthread/blob/42d026df5b07825070f60134b980a1ec2552dfee/src/pthread.c#L902)
provides the associated port. The actual installed runtime is tested separately:
1,000 registrations leave the measured send-right reference count unchanged.
No returned Mach right is deallocated by registration code.

Current registration is private TLS, zero before registration. Both registration
functions return the logical ID and update only the caller's TLS. Invalid null
or dead port names return zero; failed primary registration leaves an established
primary unchanged. The exported primary variable retains its existing
`unsigned long` type and eight-byte Mac LP64 storage. Primary replacement, as on
Windows, requires caller coordination; concurrent unsynchronized primary writes
are outside this fixture.

The retained `platform.h` has no declarations for these functions or data, and
the source tree has no callers outside `thread.cpp`. The Mac primary-registration
return is corrected from the uncompilable `pthread_t` implementation to
`unsigned int`. Existing non-template C++ exported symbol spellings and parameter
types are preserved, as are the primary data type and symbol. This does not claim
compatibility with an external binary that expected a pthread pointer return.
Windows and Linux branches remain byte-identical after source selection.

Seven cases cover initial zero state and storage widths, stable native identity,
16 concurrently live registered workers, coordinated primary replacement and
restoration without cross-thread TLS writes, 32 thread lifetimes, native port
rights, and a separate failure control. The normal fixture calls the actual
system API. Only the failure-control object renames that one API symbol to a
typed fixture binding; null/dead values and recovery through the real API test
the unchanged production method bodies. That object is labeled separately in
the command receipts. Thread IDs may be reused after a native thread exits;
only simultaneously live identities must be distinct.

Windows qualification reuses the frozen actual V11 x86/x64 Release/Debug build
receipts after proving identical Windows-selected source bytes/tokens and the
unchanged project header. No new Windows compiler invocation or VM action occurs
in this fixture. Linux runtime behavior is preserved, with no new Linux execution.
