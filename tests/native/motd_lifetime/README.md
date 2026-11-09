# MOTD lifetime diagnostics

`inspect_contexts.py` reads compiler recipes from a configured runtime Ninja
build and recompiles six complete Counter-Strike VGUI translation units with
MSVC class-layout reporting. Use a private complete frozen source tree for
`--context-root`. All object files, PDBs, logs, commands, and receipts go to
`--output`; recipe discovery does not build or edit the original producer.

The measurements cover the MOTD owner and HTML constructor, plus the Frame,
Panel, EditablePanel, and BuildGroup implementations that control layout and
child lifetime. Compiler include traces identify the actual header selected by
each translation unit. These measurements can reject a class-layout hypothesis;
they do not establish when a live HTML object was destroyed or overwritten.

Run in a matching MSVC environment, for example:

```powershell
python tests/native/motd_lifetime/inspect_contexts.py `
  --ninja C:/tools/ninja.exe --recipe-build C:/producer/build/runtime-windows-x64-debug `
  --recipe-root C:/producer --context-root C:/frozen-source --output C:/motd-layouts
```

This directory adds no runtime target, root build membership, production change,
or default output artifact.

The standalone C++14 lifetime fixture extracts the actual BrowserClose and
OpenURL methods from both HTML implementations, their HTML/Panel destructors,
Panel::MarkForDeletion, the MOTD owner's navigation/destructor methods, and the
serializer's detach/CloseResponses methods. Controlled bindings provide VGUI
registration and deferred queues, file I/O, action delivery, and browser
transport. They do not replace the methods whose lifetime contract is tested.
Complete class layout and cross-DLL execution remain separate runtime checks.

```sh
cmake -S tests/native/motd_lifetime -B build-motd-lifetime -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-motd-lifetime --parallel
ctest --test-dir build-motd-lifetime --output-on-failure
```

Use `-DCSE_MOTD_SANITIZERS=ON` for local ASan/UBSan instrumentation. The cases
verify that a detached browser's close/failure notification preserves its
owner's retained widget; URL and full HTML-file MOTDs still enter the production
navigation path; stopped-GUI notification cannot delete the child immediately;
and subsequent parent cleanup removes the stable serializer target and frees
the child and deferred context menu exactly once. Native Win32 retains its
existing close behavior. No case substitutes text for an HTML MOTD.

Pass `--html-only` to the compiler diagnostic to compile the complete HTML
implementation in CSclient, engine, and GameUI contexts after a lifetime change.
