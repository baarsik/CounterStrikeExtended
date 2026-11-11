# Final executable publication

Run on macOS without launching a game or provider:

```sh
python3 -m unittest discover -s tests/native/main_image_publication -v
```

The retained launcher/hlds SocketImageOwner item requires `socket-v1` through
`CseAppleMainImagePublisher`. Translation rejects missing, differing, unknown or
misowned requirements. The evaluated source selection adds the same rule to
Apple native64 Debug and Release; other selections do not receive it.

The rule retains all default-visible main-image publications through Darwin LTO
with `-export_dynamic`. Its mandatory post-link gate reads the final Mach-O
export trie, requires the expected thin architecture, executable file owner,
output name, and strong regular file-backed definitions of the socket host table
and CreateInterface. It rejects a local/hidden table, a table from a dependency,
a dylib supplied as the executable, and the wrong architecture/name. It does not
load the image, substitute ABI evidence or qualify initialized runtime behavior.
Receipts live only in the build directory. A checker change forces a relink;
a failed check removes an earlier success receipt.

Tests cover actual linked Debug/LTO Release controls and CMake build failure for
both owners, plus metadata selection and bounded malformed export tries. Fixture
table bytes are only publication-parser controls; no callback or admission is
executed. Production acceptance separately requires the real retained launcher
and hlds images and their existing owning ABI evidence.
