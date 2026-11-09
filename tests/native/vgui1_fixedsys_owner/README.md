# Apple FixedSys request profile

This macOS fixture uses the actual unselected Font owner with an authored fake
provider. It checks the exact FixedSys18/400 request and its case-folded spelling,
twelve neighboring requests, forwarded parameters, shared platform-cache identity
and the retained cache-hit Font ID sentinel. It does not invoke the native font provider.

Configure this directory with Ninja, build `fixedsys_owner_profiles`, then run
`ctest -R '^fixedsys-owner-profiles$'`. Use `CSE_FONT_SANITIZERS=ON` for ASan/UBSan.
The separate FixedSys role fixture qualifies raster/metric behavior. Neither test
selects a production factory or qualifies Surface, atlas, UI or readability.
