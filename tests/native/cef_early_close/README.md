# Sandboxed macOS early-close investigation

This fixture investigates browser removal during asynchronous creation using the
real pinned CEF host. It does not initialize the engine or alter production code.
Use a unique evidence directory for each invocation; execution creates an owned
temporary CEF profile and an exact fixture Keychain identity, then removes that
identity. It never requests the production storage identity.

```sh
python3 tests/native/cef_early_close/run_close_fixture.py \
  --executable /absolute/private/cse_cef_host.app/Contents/MacOS/cse_cef_host \
  --evidence /absolute/unique/evidence --mode minimal
```

The first browser loads a local HTTP page. The fixture sends the second browser
Create and Remove in one write, waits for its Close event, then navigates the
first browser again. `sized` adds Size between Create and Remove. `ready` waits
for the second browser's Ready event before removing it. Successful cases require
the navigation, normal host shutdown, stopped reader, no remaining owned helper,
and paired fixture Keychain cleanup. A missing Keychain item (security status 44)
is accepted when no item was created. Crash reports may appear after fixture exit;
collect a late report only after matching its process identity.

`build_control.py` only builds private source/app copies in a previously prepared
artifact packet. It reads the existing Debug or Release Ninja compile/link
commands and source-built CEF wrapper; it never invokes a production build. The
packet must contain `current-source/` with the frozen host source and
`current.app` with the cloned Debug app. The Release app/resources are cloned from
the selected existing Release staging directory. Every rebuilt host translation
unit comes from the private frozen source copy.

```sh
python3 tests/native/cef_early_close/build_control.py \
  --root /absolute/repository --packet /absolute/packet \
  --control candidate --configuration Release
```

Controls are single-variable comparisons against that source: `trace` records
callback and parent-destruction order, `retain-parent` keeps native parents until
after CEF shutdown, and `post-close` posts the pending close from OnAfterCreated
to the next UI task. `both` combines the latter controls. `candidate` contains
only the posted-close patch, without diagnostic instrumentation. Reusing a work
directory is rejected. No sleeps or provider modifications implement the control.

The measured pinned-provider failure reenters OnBeforeClose from the pending
Close call inside OnAfterCreated. It crashes after that callback returns and
before the queued native-parent release. Retaining the native parent still
fails; posting the pending close passes the bounded cases. The pinned CEF header
permits browser actions from OnAfterCreated, so this is an observed lifecycle
failure/workaround for the tested provider, not a general prohibition on actions
inside that callback. Stripped-framework nearest-export names are not reliable
function identities. These tests do not qualify full game browser integration,
other CEF versions, or all Cocoa teardown paths.
