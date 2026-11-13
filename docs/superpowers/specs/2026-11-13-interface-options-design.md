# Interface Options page

Add an Interface tab immediately after Video in the existing CS/HL Options dialog.
Its non-editable Language dropdown lists all ten bundled languages and an automatic
Use Steam language choice. Apply/OK saves a CSE-owned preference; Cancel/reset does
not write it. The page explains that the change takes effect after restarting.
It does not automatically interrupt a game or change Steam's language preference.

Continue the existing PropertyPage/ComboBox, CSE catalogue and startup language
resolver architecture. Keep explicit `-language` highest priority, then the saved
preference, then Steam/English. Show a launch-option notice when that override is
active. Validate saved identifiers against the bundled language table; keep the
existing command-line support for safe custom language identifiers.

Use the established native settings backends: a CSE-specific HKCU key on Windows
and the existing CSE CFPreferences domain on macOS. GameUI and startup use the same strict native
reader; GameUI writes through the existing ISystem API. Startup reads the preference
before game paths are assembled.
A failed save remains visible on the page and can be retried. Never report success
until the settings backend accepts the write.

Generate the language table from the maintained manifest, including stable native
names and readable English fallbacks for Asian names. Localize Interface, Language,
automatic, restart and failure/override notices in all ten CSE catalogues. Use a
page layout that wraps notes and keeps the dropdown/selected item visible. Adapt
the Options tab-strip width/minimum so adding the tab does not hide existing tabs
at640x480. No new external dependency or public engine/VGUI ABI slot is needed.

Live language switching would require replacing loaded game paths, fonts and HUD
state; applying on the next restart follows current startup ownership and avoids
partial translations. This is the implementation default for the user's explicit
request, not a separate approval gate.
