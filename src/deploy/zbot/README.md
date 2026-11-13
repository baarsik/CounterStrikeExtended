# ZBot runtime resources

The `cstrike/` tree contains the English ZBot profiles, chatter database and its
487 referenced radio WAV files, plus the advanced bot options page. These were
imported from the local `CS 1.6 GoldClient/cstrike` installation. The databases
retain their Michael S. Booth / Turtle Rock Studios attribution. Original input
hashes are recorded in `provenance.json`; these are existing game resources,
not newly authored CSE content.

`resource/CseZBotServerPage.res` adapts Steam Counter-Strike's create-server page:
its existing bot controls are visible and Steam Networking sits below them.
GameUI selects this layout only when ZBot is enabled for Counter-Strike.

The included `maps/de_dust2.nav` uses the supported version 5 format and records
a BSP size of 2,057,288 bytes. Its source BSP and the installed Steam CS 1.6 BSP
have the same SHA-256 recorded in `provenance.json`. Other maps retain the
existing bot navigation-learning behavior. No BSPs or player models are bundled.

`cmake/ZBot.cmake` owns the default-on `CSE_ENABLE_ZBOT` option and attaches a
single resource-staging target to the Counter-Strike server and GameUI. Staging
copies this runtime tree into the configured output root on every build,
preserves unchanged files and restores removed files. Disabling the option
skips staging and disables commands, cvars, resource loading and bot controls;
previously staged files are left in place.
