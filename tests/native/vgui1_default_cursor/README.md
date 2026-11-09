This C++14 CPU fixture compiles the complete actual DefaultCursor.cpp inside the
fixture translation unit so its anonymous CursorBitmap has its real complete
type. Cursor.cpp, Bitmap.cpp, Image.cpp and Color.cpp are complete production
translation units. It constructs their actual objects. Unrelated Panel/App/Scheme
service bodies use their real typed declarations and abort if invoked. There is
no SDL, font, GPU, provider, window or preferences dependency.

The baseline test is `check_constructor_init.py` over the actual compiler AST:
it diagnoses the dc_none early-return path lacking an explicit initializer for
both `int _hotspot[2]` cells. It executes no constructor and reads no indeterminate
value. Clang's optional UninitializedObject analyzer emitted no warning; that is
retained separately, not presented as the RED. The pre-fix executable is run ONLY
with `--ordinary`. Never run baseline `--none` or `--all`.

Post-fix `--none` verifies hotspot0/0, size0/0, null pixels, inherited Image
position/color and untouched texture/upload state. `--ordinary` covers all13
other enum values below dc_last:16x16, hotspot8/8 and every RGBA cell, including
the blue-over-green crossing. `--all` combines them. Real consumer public headers
are compiled in a separate translation unit and issue typed public calls on the
actual constructed cursor; private declaring headers compile the owners. Record
size controls are not a complete VGUI1 ABI catalog or readiness gate.

Nested allocation cleanup is FIXTURE ONLY. Existing Cursor/Bitmap/Image have no
production nested-storage cleanup. The fixture releases the actual owned pixel
array, invokes the real complete CursorBitmap destructor explicitly and releases
its allocation; it never deletes through a base with a nonvirtual destructor.
The access-tag technique obtains real protected/private field member pointers
through explicit template instantiation; it does not rewrite access specifiers,
object storage, constructors, vtables or class declarations. This does not qualify
production cold App resource ownership or unload/restart.

Run `run_macos.py --art NEW_ABSOLUTE_DIRECTORY --before PINNED_OLD_SOURCE` for
Release/Debug/ASan+UBSan, compiler-AST RED/GREEN, actual existing vgui owning recipe
before/after compiles and Windows-selected preprocessing controls. The before
source must be the retained exact original. Production owner commands preserve
architecture/minimumOS/forced header/FP/EH/RTTI flags and redirect only source and
object output. Local Windows-selected preprocessing is a source invariant, not a
new Windows MSVC compile/runtime. Windows `dc_none` remains deliberately unfixed
and is never executed by this fixture. No root source membership changes occur.
